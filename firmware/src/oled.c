#include "oled.h"

#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <util/delay.h>

#define OLED_CS_BIT PB4
#define OLED_SDI_BIT PB5
#define OLED_SCL_BIT PB7

enum oled_command {
    OLED_CLEAR_DISPLAY = 0x01U,
    OLED_RETURN_HOME = 0x02U,
    OLED_ENTRY_MODE_INCREMENT = 0x06U,
    OLED_DISPLAY_OFF = 0x08U,
    OLED_DISPLAY_ON = 0x0cU,
    OLED_FUNCTION_SET = 0x38U,
};

static void oled_write_bit(bool high)
{
    PORTB &= (uint8_t)~_BV(OLED_SCL_BIT);

    if (high) {
        PORTB |= _BV(OLED_SDI_BIT);
    } else {
        PORTB &= (uint8_t)~_BV(OLED_SDI_BIT);
    }

    PORTB |= _BV(OLED_SCL_BIT);
}

static void oled_write_byte(uint8_t value)
{
    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
        oled_write_bit((value & mask) != 0U);
    }
}

/*
 * WS0010-compatible serial writes are 10 bits: register-select, write, then
 * eight data bits. Data bytes may follow under one chip-select assertion.
 */
static void oled_send_command(enum oled_command command)
{
    PORTB &= (uint8_t)~_BV(OLED_CS_BIT);
    oled_write_bit(false); /* command */
    oled_write_bit(false); /* write */
    oled_write_byte((uint8_t)command);
    PORTB |= _BV(OLED_CS_BIT);

    /* Direct port writes outrun the WS0010 command execution time. */
    _delay_us(50);
}

void oled_clear(void)
{
    oled_send_command(OLED_CLEAR_DISPLAY);
    _delay_ms(2);
}

void oled_write_text(const char *text)
{
    PORTB &= (uint8_t)~_BV(OLED_CS_BIT);
    oled_write_bit(true);  /* data */
    oled_write_bit(false); /* write */

    while (*text != '\0') {
        oled_write_byte((uint8_t)*text++);
    }

    PORTB |= _BV(OLED_CS_BIT);
}

void oled_init(void)
{
    PORTB |= _BV(OLED_CS_BIT);
    DDRB |= _BV(OLED_CS_BIT) | _BV(OLED_SDI_BIT) | _BV(OLED_SCL_BIT);

    _delay_ms(10);
    oled_send_command(OLED_FUNCTION_SET);
    oled_send_command(OLED_DISPLAY_OFF);
    oled_clear();
    oled_send_command(OLED_ENTRY_MODE_INCREMENT);
    oled_send_command(OLED_RETURN_HOME);
    _delay_ms(2);
    oled_send_command(OLED_DISPLAY_ON);
}
