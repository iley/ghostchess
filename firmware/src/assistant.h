#ifndef ASSISTANT_H
#define ASSISTANT_H

#include "chess.h"

#define ASSISTANT_SETTLE_MS 180U
#define ASSISTANT_SETUP_MS 600U
#define ASSISTANT_GREEN_MS 350U
#define ASSISTANT_WARNING_MS 2200U
#define NO_SQUARE 64U

enum square_light { LIGHT_OFF, LIGHT_WHITE, LIGHT_GREEN, LIGHT_BLUE,
                    LIGHT_ORANGE, LIGHT_RED };

struct assistant {
    int8_t board[64];
    bool occupied[64];
    bool removed[64];
    uint16_t green_remaining[64];
    uint8_t hints[64];
    bool playing;
    bool white_turn;
    bool unresolved;
    uint8_t selected;
    uint8_t en_passant;
    bool en_passant_pending;
    uint8_t castle_rights;
    bool castle_pending;
    bool rook_only;
    uint8_t castle_from;
    uint8_t castle_to;
    bool promotion_pending;
    uint8_t promotion_from;
    uint8_t promotion_to;
    uint8_t promotion_choice;
    uint8_t warning_square;
    uint16_t warning_remaining;
    uint32_t last_update;
    uint32_t stable_since;
};

void assistant_init(struct assistant *game, uint32_t now);
/* Feed one complete, debounced snapshot, even when no sensor has changed. */
void assistant_update(struct assistant *game, const bool occupied[64],
                      uint32_t now);
/* Debounced button presses: BTN2 cycles promotion, BTN3 confirms. */
void assistant_button(struct assistant *game, uint8_t button);
enum square_light assistant_light(const struct assistant *game, uint8_t square);
const char *assistant_status(const struct assistant *game);

#endif
