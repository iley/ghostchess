#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>

#include "oled.h"
#include "assistant.h"

#define BOARD_SIZE 8U
#define LED_COUNT 64U

#define LED_DATA_BIT PB0
#define SENSOR_POWER_ON_MS 2U
#define SENSOR_POWER_OFF_MS 2U
#define SENSOR_SAMPLE_COUNT 8U
#define SENSOR_SAMPLE_INTERVAL_US 100U
#define SENSOR_DEBOUNCE_SCANS 3U
#define BASE_BRIGHTNESS 6U
#define HINT_BRIGHTNESS 48U

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
static struct assistant game;

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

    /* BTN1: hold for two seconds to start a new standard game. */
    DDRD &= (uint8_t)~_BV(PD5);
    PORTD |= _BV(PD5);
}

static uint8_t led_index(uint8_t row, uint8_t file)
{
    /* LED data reverses direction on ranks 7, 5, 3, and 1. */
    uint8_t serial_file = ((row & 1U) == 0U) ? file : (7U - file);
    return (uint8_t)(row * BOARD_SIZE + serial_file);
}

/* Poll a free-running timer: LED transmission cannot lose timer interrupts.
 * Timer1 / 256 = 43200 Hz. Read at least once per 1.5-second wrap. */
static uint32_t milliseconds(void)
{
    static uint16_t previous;
    static uint16_t remainder;
    static uint32_t now;
    uint16_t ticks = TCNT1;
    uint16_t elapsed = (uint16_t)(ticks - previous);
    previous = ticks;
    uint32_t scaled = (uint32_t)elapsed * 10U + remainder;
    now += scaled / 432U;
    remainder = scaled % 432U;
    return now;
}

static bool render_board(void)
{
    bool changed = false;
    for (uint8_t row = 0U; row < BOARD_SIZE; ++row) {
        for (uint8_t file = 0U; file < BOARD_SIZE; ++file) {
            uint8_t square = (uint8_t)(row * BOARD_SIZE + file);
            struct pixel *pixel = &pixels[led_index(row, file)];
            struct pixel next = { 0, 0, 0 };
            switch (assistant_light(&game, square)) {
            case LIGHT_WHITE:
                next.red = next.green = next.blue = BASE_BRIGHTNESS;
                break;
            case LIGHT_GREEN: next.green = HINT_BRIGHTNESS; break;
            case LIGHT_BLUE: next.blue = HINT_BRIGHTNESS; break;
            case LIGHT_ORANGE:
                next.red = HINT_BRIGHTNESS;
                next.green = HINT_BRIGHTNESS / 3U;
                break;
            case LIGHT_RED: next.red = HINT_BRIGHTNESS; break;
            default: break;
            }
            if (pixel->red != next.red || pixel->green != next.green ||
                pixel->blue != next.blue) changed = true;
            *pixel = next;
        }
    }
    return changed;
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
    hardware_init();
    assistant_init(&game, 0);
    render_board();
    ws2812_show(pixels, LED_COUNT);
    oled_init();

    TCNT1 = 0;
    TCCR1A = 0;
    TCCR1B = _BV(CS12); /* Timer1 prescaler 256. */
    const char *last_status = 0;
    bool button_down = false;
    bool reset_done = false;
    uint32_t button_since = 0;

    for (;;) {
        scan_sensors();
        uint32_t now = milliseconds();
        if ((PIND & _BV(PD5)) == 0) {
            if (!button_down) {
                button_down = true;
                button_since = now;
            }
            if (!reset_done && now - button_since >= 2000U) {
                assistant_init(&game, now);
                reset_done = true;
            }
        } else {
            button_down = false;
            reset_done = false;
        }

        assistant_update(&game, sensor_active, now);
        if (render_board()) ws2812_show(pixels, LED_COUNT);
        const char *status = assistant_status(&game);
        if (status != last_status) {
            oled_clear();
            oled_write_text(status);
            last_status = status;
        }
    }
}
