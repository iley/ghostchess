#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>

#define BOARD_SIZE 8U
#define LED_COUNT 64U

#define LED_DATA_BIT PB0
#define SENSOR_SETTLE_US 40U
#define WHITE_HALF 128U
#define GREEN_FULL 255U

/* Timer 1 runs at exactly 43.2 kHz with the 11.0592 MHz clock / 256. */
#define TIMER1_HZ (F_CPU / 256UL)
#define BLINK_TICKS ((uint16_t)((TIMER1_HZ * 500UL) / 1000UL))

#if F_CPU != 11059200UL
#error "The WS2812 timing and board timer assume an 11.0592 MHz CPU clock"
#endif

/* WS2812 byte order on the wire is green, red, blue. */
struct pixel {
    uint8_t green;
    uint8_t red;
    uint8_t blue;
} __attribute__((packed));

static struct pixel pixels[LED_COUNT];
static bool blink_active[LED_COUNT];
static uint16_t blink_until[LED_COUNT];
static uint8_t previous_detected[BOARD_SIZE];

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

    /* Free-running timebase; 65536 ticks wrap after about 1.5 seconds. */
    TCCR1A = 0x00U;
    TCCR1B = 0x00U;
    TCNT1 = 0U;
    TCCR1B = _BV(CS12); /* clk/256 */
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

            if (blink_active[square]) {
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

static bool time_reached(uint16_t now, uint16_t deadline)
{
    return (int16_t)(now - deadline) >= 0;
}

static bool expire_blinks(uint16_t now)
{
    bool changed = false;

    for (uint8_t square = 0U; square < LED_COUNT; ++square) {
        if (blink_active[square] && time_reached(now, blink_until[square])) {
            blink_active[square] = false;
            changed = true;
        }
    }

    return changed;
}

static bool scan_sensors(uint16_t now)
{
    bool changed = false;

    for (uint8_t row = 0U; row < BOARD_SIZE; ++row) {
        /* Row 0 is chess rank 8 / SENSOR_ON_1 / PC7. */
        PORTC = _BV(7U - row);
        _delay_us(SENSOR_SETTLE_US);

        uint8_t detected = (uint8_t)~PINA;
        PORTC = 0x00U;

        uint8_t newly_detected = detected & (uint8_t)~previous_detected[row];
        previous_detected[row] = detected;

        for (uint8_t file = 0U; file < BOARD_SIZE; ++file) {
            if ((newly_detected & _BV(file)) != 0U) {
                uint8_t square = (uint8_t)(row * BOARD_SIZE + file);
                blink_active[square] = true;
                blink_until[square] = (uint16_t)(now + BLINK_TICKS);
                changed = true;
            }
        }
    }

    return changed;
}

int main(void)
{
    hardware_init();

    render_board();
    ws2812_show(pixels, LED_COUNT);

    for (;;) {
        uint16_t now = TCNT1;
        bool display_changed = scan_sensors(now);
        display_changed |= expire_blinks(TCNT1);

        if (display_changed) {
            render_board();
            ws2812_show(pixels, LED_COUNT);
        }
    }
}
