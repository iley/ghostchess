#ifndef OLED_H
#define OLED_H

#include <stdint.h>

/* Initialize the OLED and leave it on with an empty display. */
void oled_init(void);

/* Clear the display and return the cursor to the first character. */
void oled_clear(void);

/* Move the cursor to a row (0 or 1) and column. */
void oled_set_cursor(uint8_t row, uint8_t column);

/* Write a null-terminated string at the current cursor position. */
void oled_write_text(const char *text);

#endif
