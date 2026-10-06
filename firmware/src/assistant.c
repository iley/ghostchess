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
    game->castle_rights = CHESS_CASTLE_ALL;
    game->selected = NO_SQUARE;
    game->en_passant = CHESS_NO_SQUARE;
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
    game->gesture = GESTURE_NONE;
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
        if (selected == (game->white_turn ? 60 : 4) &&
            (to == selected - 2 || to == selected + 2) &&
            chess_legal_castle(game->board, game->white_turn, to > selected,
                               game->castle_rights))
            game->hints[to] = LIGHT_BLUE;
        if (chess_legal_en_passant(game->board, selected, to, game->en_passant))
            game->hints[to] = LIGHT_ORANGE;
    }
}

/* Record history against the original position, before moving any pieces. */
static void record_move(struct assistant *game, uint8_t from, uint8_t to,
                        bool legal, uint8_t next_en_passant)
{
    if (!legal) {
        game->warning_square = to;
        game->warning_remaining = ASSISTANT_WARNING_MS;
    }
    game->castle_rights = chess_rights_after(game->board, from, to,
                                            game->castle_rights);
    game->en_passant = next_en_passant;
    game->white_turn = game->board[from] < 0;
    clear_gesture(game);
}

static void commit_move(struct assistant *game, uint8_t from, uint8_t to,
                        uint8_t victim, uint8_t promotion)
{
    bool white = game->board[from] > 0;
    bool legal = white == game->white_turn &&
        (victim != NO_SQUARE ?
         chess_legal_en_passant(game->board, from, to, game->en_passant) :
         chess_legal(game->board, from, to));
    uint8_t next_en_passant = victim == NO_SQUARE && legal ?
        chess_en_passant_target(game->board, from, to) : CHESS_NO_SQUARE;
    record_move(game, from, to, legal, next_en_passant);
    if (victim != NO_SQUARE) game->board[victim] = EMPTY;
    chess_move(game->board, from, to);
    if (promotion != EMPTY) game->board[to] = white ? promotion : -promotion;
}

/* Preserve both pieces until the entire castle is observed. Legality is
 * advisory; rights and attacks do not change the physical gesture shape. */
static bool handle_castle(struct assistant *game)
{
    for (uint8_t variant = 0; variant < 4; ++variant) {
        bool white = variant < 2, kingside = (variant & 1U) == 0;
        if (!chess_castle_shape(game->board, white, kingside)) continue;
        uint8_t row = white ? 56 : 0;
        uint8_t king = row + 4, rook = row + (kingside ? 7 : 0);
        uint8_t kt = row + (kingside ? 6 : 2);
        uint8_t rt = row + (kingside ? 5 : 3);
        if (!game->occupied[kt] && !game->occupied[rt]) continue;
        /* Associate each landing with its lifted source. A king on f1/d1
         * or a rook on g1/c1 is an ordinary move, not a partial castle. */
        if ((game->occupied[kt] && game->occupied[king]) ||
            (game->occupied[rt] && game->occupied[rook])) continue;
        bool compatible = true;
        for (uint8_t s = 0; s < 64; ++s) {
            if (s == king || s == rook || s == kt || s == rt) continue;
            if (game->occupied[s] != (game->board[s] != EMPTY) ||
                game->removed[s]) compatible = false;
        }
        if (!compatible) continue;
        game->unresolved = false;
        if (!game->occupied[king] && !game->occupied[rook] &&
            game->occupied[kt] && game->occupied[rt]) {
            bool legal = white == game->white_turn &&
                chess_legal_castle(game->board, white, kingside, game->castle_rights);
            record_move(game, king, kt, legal, CHESS_NO_SQUARE);
            chess_move(game->board, king, kt);
            chess_move(game->board, rook, rt);
        } else {
            game->gesture = game->occupied[king] && !game->occupied[rook] &&
                            !game->occupied[kt] && game->occupied[rt] ?
                            GESTURE_ROOK_OR_CASTLE : GESTURE_CASTLE;
            game->gesture_from = rook;
            game->gesture_to = rt;
        }
        return true;
    }
    return false;
}

static bool promotion_matches(const struct assistant *game, bool allow_lift)
{
    for (uint8_t s = 0; s < 64; ++s) {
        if (s == game->gesture_to && allow_lift) continue;
        bool expected = s == game->gesture_to ||
                        (s != game->gesture_from && game->board[s] != EMPTY);
        if (game->occupied[s] != expected) return false;
    }
    return true;
}

void assistant_button(struct assistant *game, uint8_t button)
{
    if (!game->playing || game->unresolved ||
        game->last_update - game->stable_since < ASSISTANT_SETTLE_MS) return;
    if (game->gesture == GESTURE_PROMOTION) {
        static const uint8_t choices[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
        if (button == 2) game->promotion_choice = (game->promotion_choice + 1) % 4;
        if (button == 3 && promotion_matches(game, false))
            commit_move(game, game->gesture_from, game->gesture_to,
                        NO_SQUARE, choices[game->promotion_choice]);
    } else if (button == 3 && game->gesture == GESTURE_ROOK_OR_CASTLE) {
        commit_move(game, game->gesture_from, game->gesture_to, NO_SQUARE, EMPTY);
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
            if (occupied[s] && (game->playing || game->board[s] != EMPTY))
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

    if (game->gesture == GESTURE_PROMOTION) {
        game->unresolved = !promotion_matches(game, true);
        return;
    }

    /* Reclassify from this snapshot; never confirm stale castle state. */
    game->gesture = GESTURE_NONE;
    update_hints(game);
    if (now - game->stable_since < ASSISTANT_SETTLE_MS) return;

    if (handle_castle(game)) return;

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

    /* A diagonal pawn landing can precede either removal. Keep the tracked
     * position intact until all three squares agree; a pause is not a turn. */
    uint8_t ep_from = NO_SQUARE, ep_victim = NO_SQUARE, ep_count = 0;
    if (added_count == 1 && replaced_count == 0) {
        for (uint8_t from = 0; from < 64; ++from) {
            uint8_t victim = chess_en_passant_victim(game->board, from, added);
            if (victim == CHESS_NO_SQUARE) continue;
            uint8_t participants_missing = (!occupied[from]) + (!occupied[victim]);
            if (participants_missing == 0 || participants_missing != missing_count)
                continue;
            ep_from = from;
            ep_victim = victim;
            ++ep_count;
        }
    }
    if (ep_count != 0 && (ep_count != 1 || missing_count != 2)) {
        game->gesture = GESTURE_EN_PASSANT;
        game->unresolved = false;
        return;
    }

    uint8_t to = NO_SQUARE;
    if (missing_count == 1 && added_count == 1 && replaced_count == 0)
        to = added;
    /* Either restoration order after two lifts is indistinguishable from a
     * capture. Only full restoration before settling cancels it reliably. */
    if (missing_count == 1 && added_count == 0 && replaced_count == 1)
        to = replaced;
    if (ep_count == 1 && missing_count == 2) {
        missing = ep_from;
        to = added;
    }

    if (to == NO_SQUARE) {
        /* A single lifted piece, or both participants in a capture, can wait
         * indefinitely. More complex handling needs restoration, not guesses. */
        game->unresolved = added_count != 0 || missing_count > 2 ||
                           replaced_count > 1;
        return;
    }

    bool white = game->board[missing] > 0;
    if (game->board[missing] == (white ? PAWN : -PAWN) &&
        to / 8 == (white ? 0 : 7)) {
        game->gesture = GESTURE_PROMOTION;
        game->gesture_from = missing;
        game->gesture_to = to;
        game->promotion_choice = 0;
        game->unresolved = false;
        memset(game->hints, 0, sizeof game->hints);
        return;
    }
    /* Legality is advisory. Follow the observed move, including wrong turns. */
    commit_move(game, missing, to,
                ep_count == 1 && missing_count == 2 ? ep_victim : NO_SQUARE,
                EMPTY);
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
    if (game->occupied[square] && game->green_remaining[square] != 0)
        return LIGHT_GREEN;
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
    if (game->gesture == GESTURE_PROMOTION) {
        if (!game->occupied[game->gesture_to]) return "Place promotion";
        static const char *const prompts[4] = {
            "Queen 2next 3OK", "Rook 2next 3OK",
            "Bishop 2next 3OK", "Knight 2next 3OK"
        };
        return prompts[game->promotion_choice];
    }
    if (game->gesture == GESTURE_ROOK_OR_CASTLE) return "Castle/3 rook";
    if (game->gesture == GESTURE_CASTLE) return "Finish castling";
    if (game->gesture == GESTURE_EN_PASSANT) return "Finish en passant";
    return game->white_turn ? "White to move" : "Black to move";
}
