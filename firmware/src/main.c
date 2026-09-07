#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>

#include "oled.h"

#define BOARD_SIZE 8U
#define LED_COUNT 64U

#define LED_DATA_BIT PB0
#define SENSOR_POWER_ON_MS 2U
#define SENSOR_POWER_OFF_MS 2U
#define SENSOR_SAMPLE_COUNT 8U
#define SENSOR_SAMPLE_INTERVAL_US 100U
#define SENSOR_DEBOUNCE_SCANS 3U
#define WHITE_HALF 128U
#define GREEN_FULL 255U

#if F_CPU != 11059200UL
#error "The WS2812 timing assumes an 11.0592 MHz CPU clock"
#endif

/* WS2812 byte order on the wire is green, red, blue. */
struct pixel {
    uint8_t green;
    uint8_t red;
    uint8_t blue;
} __attribute__((packed));

static struct pixel pixels[LED_COUNT];
static bool sensor_active[LED_COUNT];
static uint8_t sensor_confidence[LED_COUNT];

/*
 * Send one WS2812 frame on PB0.
 *
 * The inner loop is 14 CPU cycles per bit (1.266 us). A zero stays high for
 * 4 cycles (0.362 us); a one stays high for 9 cycles (0.814 us). Interrupts
 * remain disabled for the approximately 2 ms needed to send all 64 pixels.
 */
static void ws2812_show(const struct pixel *data, uint16_t count)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint16_t byte_count = count * 3U;
    uint8_t high = PORTB | _BV(LED_DATA_BIT);
    uint8_t low = PORTB & (uint8_t)~_BV(LED_DATA_BIT);
    uint8_t saved_sreg = SREG;

    cli();

    while (byte_count-- != 0U) {
        uint8_t value = *bytes++;
        uint8_t bit_count;

        __asm__ volatile(
            "ldi %[bit_count], 8"        "\n\t"
            "1:"                        "\n\t"
            "out %[port], %[high]"      "\n\t"
            "nop"                       "\n\t"
            "nop"                       "\n\t"
            "sbrs %[value], 7"          "\n\t"
            "out %[port], %[low]"       "\n\t"
            "nop"                       "\n\t"
            "nop"                       "\n\t"
            "nop"                       "\n\t"
            "nop"                       "\n\t"
            "out %[port], %[low]"       "\n\t"
            "lsl %[value]"              "\n\t"
            "dec %[bit_count]"          "\n\t"
            "brne 1b"                   "\n\t"
            : [bit_count] "=&d"(bit_count), [value] "+r"(value)
            : [port] "I"(_SFR_IO_ADDR(PORTB)), [high] "r"(high),
              [low] "r"(low)
            : "cc", "memory");
    }

    SREG = saved_sreg;
    _delay_us(80);
}

/* JTD must be written twice within four CPU cycles. */
static void disable_jtag(void)
{
    uint8_t mcucr = MCUCR | _BV(JTD);

    __asm__ volatile(
        "out %[reg_addr], %[value]" "\n\t"
        "out %[reg_addr], %[value]" "\n\t"
        :
        : [reg_addr] "I"(_SFR_IO_ADDR(MCUCR)), [value] "r"(mcucr)
        : "memory");
}

static void hardware_init(void)
{
    /* Make F_CPU true even when the CKDIV8 fuse is programmed. */
    clock_prescale_set(clock_div_1);

    PORTB &= (uint8_t)~_BV(LED_DATA_BIT);
    DDRB |= _BV(LED_DATA_BIT);

    /* PA0..PA7 are active-low, open-drain sensor inputs. */
    DDRA = 0x00U;
    PORTA = 0xffU;

    /* PC7..PC0 are rank enables. Start with every rank switched off. */
    PORTC = 0x00U;
    disable_jtag();
    DDRC = 0xffU;
}

static uint8_t led_index(uint8_t row, uint8_t file)
{
    /* LED data reverses direction on ranks 7, 5, 3, and 1. */
    uint8_t serial_file = ((row & 1U) == 0U) ? file : (7U - file);
    return (uint8_t)(row * BOARD_SIZE + serial_file);
}

static void render_board(void)
{
    for (uint8_t row = 0U; row < BOARD_SIZE; ++row) {
        for (uint8_t file = 0U; file < BOARD_SIZE; ++file) {
            uint8_t square = (uint8_t)(row * BOARD_SIZE + file);
            struct pixel *pixel = &pixels[led_index(row, file)];

            if (sensor_active[square]) {
                pixel->green = GREEN_FULL;
                pixel->red = 0U;
                pixel->blue = 0U;
            } else if (((row + file) & 1U) == 0U) {
                /* A8 is a light square; A1 is a dark square. */
                pixel->green = WHITE_HALF;
                pixel->red = WHITE_HALF;
                pixel->blue = WHITE_HALF;
            } else {
                pixel->green = 0U;
                pixel->red = 0U;
                pixel->blue = 0U;
            }
        }
    }
}

static bool scan_sensors(void)
{
    bool changed = false;

    for (uint8_t row = 0U; row < BOARD_SIZE; ++row) {
        /* A low bus before selection still belongs to an earlier row. */
        uint8_t idle_high = PINA;

        /* Row 0 is chess rank 8 / SENSOR_ON_1 / PC7. */
        PORTC = _BV(7U - row);
        _delay_ms(SENSOR_POWER_ON_MS);

        uint8_t detected = (uint8_t)~PINA;

        /* Reject short noise pulses on the weakly pulled-up file buses. */
        for (uint8_t sample = 1U; sample < SENSOR_SAMPLE_COUNT; ++sample) {
            _delay_us(SENSOR_SAMPLE_INTERVAL_US);
            detected &= (uint8_t)~PINA;
        }

        PORTC = 0x00U;

        /* Let the previous row release the shared open-drain file buses. */
        _delay_ms(SENSOR_POWER_OFF_MS);

        for (uint8_t file = 0U; file < BOARD_SIZE; ++file) {
            uint8_t square = (uint8_t)(row * BOARD_SIZE + file);
            bool detected_now = (detected & _BV(file)) != 0U;

            if ((idle_high & _BV(file)) == 0U) {
                continue;
            }

            if (detected_now) {
                if (sensor_confidence[square] < SENSOR_DEBOUNCE_SCANS) {
                    ++sensor_confidence[square];
                }

                if (sensor_confidence[square] == SENSOR_DEBOUNCE_SCANS &&
                    !sensor_active[square]) {
                    sensor_active[square] = true;
                    changed = true;
                }
            } else {
                if (sensor_confidence[square] > 0U) {
                    --sensor_confidence[square];
                }

                if (sensor_confidence[square] == 0U && sensor_active[square]) {
                    sensor_active[square] = false;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

int main(void)
{
    static const char boot_message[] = "Ghost Chess v1";

    hardware_init();

    render_board();
    ws2812_show(pixels, LED_COUNT);

    oled_init();
    oled_write_text(boot_message);
    _delay_ms(1000);
    oled_clear();

    for (;;) {
        bool display_changed = scan_sensors();

        if (display_changed) {
            render_board();
            ws2812_show(pixels, LED_COUNT);
        }
    }
}
