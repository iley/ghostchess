#include "buttons.h"

void buttons_update(struct buttons *buttons, struct assistant *game,
                    uint8_t pressed, uint32_t now)
{
    uint8_t rising = 0;
    for (uint8_t i = 0; i < 3; ++i) {
        uint8_t bit = (uint8_t)(1U << i);
        if ((pressed & bit) != (buttons->candidate & bit)) {
            buttons->candidate ^= bit;
            buttons->changed_at[i] = now;
        }
        if ((buttons->candidate & bit) != (buttons->down & bit) &&
            now - buttons->changed_at[i] >= BUTTON_DEBOUNCE_MS) {
            buttons->down ^= bit;
            if (buttons->down & bit) rising |= bit;
        }
    }
    if (rising & 1U) buttons->reset_since = now;
    if (!(buttons->down & 1U)) buttons->reset_done = false;
    if ((buttons->down & 1U) && !buttons->reset_done &&
        now - buttons->reset_since >= BUTTON_RESET_MS) {
        assistant_init(game, now);
        buttons->reset_done = true;
        return;
    }
    /* A reset hold and simultaneous choice/confirm presses are not choices. */
    if (!(buttons->down & 1U)) {
        if (rising == 2U && buttons->down == 2U && pressed == 2U) assistant_button(game, 2);
        if (rising == 4U && buttons->down == 4U && pressed == 4U) assistant_button(game, 3);
    }
}
