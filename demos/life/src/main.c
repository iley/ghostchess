#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <util/delay.h>

#include "app.h"
#include "oled.h"

#define BOARD_SIZE 8U
#define LED_COUNT 64U
#define LED_DATA_BIT PB0

#if F_CPU != 11059200UL
#error "The WS2812 timing assumes an 11.0592 MHz CPU clock"
#endif

/* WS2812 byte order on the wire is green, red, blue. */
struct pixel {
    uint8_t green;
    uint8_t red;
    uint8_t blue;
} __attribute__((packed));

static const struct pixel ALIVE = { .green = 155, .red = 155, .blue = 155 };
/* Newborn cells start dim green and fade up to ALIVE. */
static const struct pixel BORN = { .green = 28, .red = 0, .blue = 0 };

static struct pixel pixels[LED_COUNT];
static struct app app;
static uint32_t EEMEM seed_store;

static void hardware_init(void);
static void disable_jtag(void);
static uint32_t boot_seed(void);
static uint32_t milliseconds(void);
static uint8_t read_buttons(void);
static bool render(const struct app *app, uint32_t now);
static struct pixel blend(struct pixel from, struct pixel to, uint8_t amount);
static uint8_t blend_channel(uint8_t from, uint8_t to, uint8_t amount);
static uint8_t led_index(uint8_t row, uint8_t file);
static void ws2812_show(const struct pixel *data, uint16_t count);
static void show_line(uint8_t row, char *shown, const char *text);

int main(void)
{
    hardware_init();
    /* Clear whatever the LEDs latched before the reset. */
    ws2812_show(pixels, LED_COUNT);
    oled_init();
    app_init(&app, boot_seed(), milliseconds());

    /* The OLED starts blank, so any differing text is written on the first pass. */
    char top[APP_COLUMNS + 1U], bottom[APP_COLUMNS + 1U];
    char shown_top[APP_COLUMNS + 1U] = "", shown_bottom[APP_COLUMNS + 1U] = "";
    for (;;) {
        uint32_t now = milliseconds();
        app_update(&app, read_buttons(), now);
        if (render(&app, now)) ws2812_show(pixels, LED_COUNT);
        app_text(&app, top, bottom);
        show_line(0, shown_top, top);
        show_line(1, shown_bottom, bottom);
    }
}

static void hardware_init(void)
{
    /* Make F_CPU true even when the CKDIV8 fuse is programmed. */
    clock_prescale_set(clock_div_1);

    PORTB &= (uint8_t)~_BV(LED_DATA_BIT);
    DDRB |= _BV(LED_DATA_BIT);

    /* Keep every sensor rank switched off; the demo does not scan them. JTAG
     * shares PC2..PC5 and would otherwise leave those ranks floating. */
    PORTC = 0x00U;
    disable_jtag();
    DDRC = 0xffU;

    /* Active-low buttons BTN1..BTN3 with internal pull-ups. */
    DDRD &= (uint8_t)~(_BV(PD5) | _BV(PD6) | _BV(PD7));
    PORTD |= _BV(PD5) | _BV(PD6) | _BV(PD7);

    TCNT1 = 0;
    TCCR1A = 0;
    TCCR1B = _BV(CS12); /* Timer1 prescaler 256. */
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

/* Nothing on the board produces entropy at power-on, so a counter in EEPROM
 * makes each boot start from a different pattern. */
static uint32_t boot_seed(void)
{
    uint32_t seed = eeprom_read_dword(&seed_store) + 0x9e3779b9U;
    eeprom_update_dword(&seed_store, seed);
    return seed;
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

static uint8_t read_buttons(void)
{
    return (uint8_t)(((uint8_t)~PIND >> PD5) & 7U);
}

static bool render(const struct app *app, uint32_t now)
{
    static const struct pixel off = { 0, 0, 0 };
    const uint8_t *grid = life_current(&app->life);
    uint8_t fade = app_fade(app, now);
    /* LED output is linear but perceived brightness is not; squaring the
     * progress makes the fades look even instead of rushing at the dim end. */
    uint8_t rising = (uint8_t)((uint16_t)fade * fade / 255U);
    uint8_t falling = (uint8_t)((uint16_t)(255U - fade) * (255U - fade) / 255U);
    bool changed = false;
    for (uint8_t row = 0; row < BOARD_SIZE; ++row) {
        for (uint8_t file = 0; file < BOARD_SIZE; ++file) {
            bool alive = (grid[row] >> file) & 1U;
            bool was_alive = (app->from[row] >> file) & 1U;
            struct pixel next = off;
            if (alive) {
                next = was_alive ? ALIVE : blend(BORN, ALIVE, rising);
            } else if (was_alive) {
                next = blend(off, ALIVE, falling);
            }
            struct pixel *pixel = &pixels[led_index(row, file)];
            if (memcmp(pixel, &next, sizeof next) != 0) changed = true;
            *pixel = next;
        }
    }
    return changed;
}

static struct pixel blend(struct pixel from, struct pixel to, uint8_t amount)
{
    struct pixel result;
    result.green = blend_channel(from.green, to.green, amount);
    result.red = blend_channel(from.red, to.red, amount);
    result.blue = blend_channel(from.blue, to.blue, amount);
    return result;
}

static uint8_t blend_channel(uint8_t from, uint8_t to, uint8_t amount)
{
    /* 32 bits: 255 * 255 overflows the AVR's 16-bit int. */
    return (uint8_t)(from + ((int32_t)to - from) * amount / 255);
}

static uint8_t led_index(uint8_t row, uint8_t file)
{
    /* LED data reverses direction on ranks 7, 5, 3, and 1. */
    uint8_t serial_file = ((row & 1U) == 0U) ? file : (7U - file);
    return (uint8_t)(row * BOARD_SIZE + serial_file);
}

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

/* Rewrite a line in place only when it changed: clearing the display takes
 * 7 ms and flickers. */
static void show_line(uint8_t row, char *shown, const char *text)
{
    if (strcmp(shown, text) == 0) return;
    oled_set_cursor(row, 0);
    oled_write_text(text);
    strcpy(shown, text);
}
