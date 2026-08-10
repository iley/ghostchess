#ifndef OLED_H
#define OLED_H

/* Initialize the OLED and leave it on with an empty display. */
void oled_init(void);

/* Clear the display and return the cursor to the first character. */
void oled_clear(void);

/* Write a null-terminated string at the current cursor position. */
void oled_write_text(const char *text);

#endif
