#ifndef BUTTONS_H
#define BUTTONS_H

#include "assistant.h"

#define BUTTON_DEBOUNCE_MS 50U
#define BUTTON_RESET_MS 2000U
struct buttons {
    uint8_t candidate;
    uint8_t down;
    uint32_t changed_at[3];
    uint32_t reset_since;
    bool reset_done;
};

/* Zero-initialize once. Bits 0..2 represent pressed BTN1..BTN3. Call after
 * assistant_update so confirmation always uses the latest sensor snapshot. */
void buttons_update(struct buttons *buttons, struct assistant *game,
                    uint8_t pressed, uint32_t now);
#endif
