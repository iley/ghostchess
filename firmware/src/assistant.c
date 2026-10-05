#include "assistant.h"

#include <string.h>

static uint16_t countdown(uint16_t remaining, uint32_t elapsed)
{
    return elapsed >= remaining ? 0 : remaining - (uint16_t)elapsed;
}

void assistant_init(struct assistant *game, uint32_t now)
{
    memset(game, 0, sizeof *game);
    chess_start(game->board);
    game->white_turn = true;
    game->selected = NO_SQUARE;
    game->warning_square = NO_SQUARE;
    game->last_update = now;
    game->stable_since = now;
}

static void clear_gesture(struct assistant *game)
{
    memset(game->removed, 0, sizeof game->removed);
    memset(game->hints, 0, sizeof game->hints);
    game->selected = NO_SQUARE;
    game->unresolved = false;
}

static void update_hints(struct assistant *game)
{
    uint8_t selected = NO_SQUARE;
    uint8_t count = 0;
    for (uint8_t s = 0; s < 64; ++s) {
        if (game->board[s] != EMPTY && !game->occupied[s] &&
            (game->board[s] > 0) == game->white_turn) {
            selected = s;
            ++count;
        }
    }
    if (count != 1) selected = NO_SQUARE;
    if (selected == game->selected) return;
    game->selected = selected;
    memset(game->hints, 0, sizeof game->hints);
    if (selected == NO_SQUARE) return;
    for (uint8_t to = 0; to < 64; ++to) {
        if (chess_legal(game->board, selected, to))
            game->hints[to] = game->board[to] == EMPTY ? LIGHT_BLUE : LIGHT_ORANGE;
    }
}

void assistant_update(struct assistant *game, const bool occupied[64],
                      uint32_t now)
{
    uint32_t elapsed = now - game->last_update;
    game->last_update = now;
    game->warning_remaining = countdown(game->warning_remaining, elapsed);
    bool changed = false;
    bool matches = true;
    for (uint8_t s = 0; s < 64; ++s) {
        game->green_remaining[s] = countdown(game->green_remaining[s], elapsed);
        if (occupied[s] != game->occupied[s]) {
            changed = true;
            if (!game->playing && occupied[s] && game->board[s] != EMPTY)
                game->green_remaining[s] = ASSISTANT_GREEN_MS;
            if (game->playing && !occupied[s] && game->board[s] != EMPTY)
                game->removed[s] = true;
            game->occupied[s] = occupied[s];
        }
        if (occupied[s] != (game->board[s] != EMPTY)) matches = false;
    }
    if (changed) game->stable_since = now;

    if (!game->playing) {
        if (matches && now - game->stable_since >= ASSISTANT_SETUP_MS) {
            game->playing = true;
            clear_gesture(game);
        }
        return;
    }

    /* A complete restoration cancels without consuming a turn. */
    if (matches) {
        clear_gesture(game);
        return;
    }

    update_hints(game);
    if (now - game->stable_since < ASSISTANT_SETTLE_MS) return;

    uint8_t missing = NO_SQUARE, missing_count = 0;
    uint8_t added = NO_SQUARE, added_count = 0;
    uint8_t replaced = NO_SQUARE, replaced_count = 0;
    for (uint8_t s = 0; s < 64; ++s) {
        if (game->board[s] != EMPTY && !occupied[s]) {
            missing = s;
            ++missing_count;
        } else if (game->board[s] == EMPTY && occupied[s]) {
            added = s;
            ++added_count;
        } else if (game->board[s] != EMPTY && occupied[s] && game->removed[s]) {
            replaced = s;
            ++replaced_count;
        }
    }

    uint8_t to = NO_SQUARE;
    if (missing_count == 1 && added_count == 1 && replaced_count == 0)
        to = added;
    if (missing_count == 1 && added_count == 0 && replaced_count == 1)
        to = replaced;

    if (to == NO_SQUARE) {
        /* A single lifted piece, or both participants in a capture, can wait
         * indefinitely. More complex handling needs restoration, not guesses. */
        game->unresolved = added_count != 0 || missing_count > 2 ||
                           replaced_count > 1;
        return;
    }

    bool white = game->board[missing] > 0;
    bool legal = white == game->white_turn && chess_legal(game->board, missing, to);
    if (!legal) {
        game->warning_square = to;
        game->warning_remaining = ASSISTANT_WARNING_MS;
    }
    /* Legality is advisory. Follow the observed move, including wrong-turn
     * moves, and make the opposite color next. */
    chess_move(game->board, missing, to);
    game->white_turn = !white;
    clear_gesture(game);
}

enum square_light assistant_light(const struct assistant *game, uint8_t square)
{
    if (square >= 64) return LIGHT_OFF;
    enum square_light base = ((square / 8 + square % 8) & 1U) == 0 ?
                             LIGHT_WHITE : LIGHT_OFF;
    if (!game->playing) {
        if (game->occupied[square] && game->board[square] == EMPTY) return LIGHT_RED;
        if (game->occupied[square] && game->green_remaining[square] != 0)
            return LIGHT_GREEN;
        return base;
    }
    if (square == game->warning_square && game->warning_remaining != 0)
        return ((ASSISTANT_WARNING_MS - game->warning_remaining) / 220U) % 2 == 0 ?
               LIGHT_RED : base;
    if (game->hints[square] != 0) return (enum square_light)game->hints[square];
    if ((game->board[square] == KING && chess_in_check(game->board, true)) ||
        (game->board[square] == -KING && chess_in_check(game->board, false)))
        return LIGHT_ORANGE;
    return base;
}

const char *assistant_status(const struct assistant *game)
{
    if (!game->playing) return "Set up pieces";
    if (game->unresolved) return "Restore pieces";
    return game->white_turn ? "White to move" : "Black to move";
}
