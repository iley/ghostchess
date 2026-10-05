#include "assistant.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SQ(file, rank) ((uint8_t)((8 - (rank)) * 8 + ((file) - 'a')))

static struct assistant game;
static bool sensors[64];
static uint32_t now;

static void tick(uint32_t ms)
{
    now += ms;
    assistant_update(&game, sensors, now);
}

static void sensor(uint8_t square, bool occupied)
{
    sensors[square] = occupied;
    tick(40);
}

static void start(void)
{
    now = 0;
    memset(sensors, 0, sizeof sensors);
    assistant_init(&game, now);
    for (uint8_t s = 0; s < 64; ++s) sensors[s] = game.board[s] != EMPTY;
    tick(40);
    assert(!game.playing);
    tick(ASSISTANT_SETUP_MS);
    assert(game.playing && game.white_turn);
}

static void position(const int8_t board[64], bool white)
{
    start();
    memcpy(game.board, board, 64);
    for (uint8_t s = 0; s < 64; ++s)
        sensors[s] = game.occupied[s] = board[s] != EMPTY;
    game.white_turn = white;
}

static void move(uint8_t from, uint8_t to)
{
    sensor(from, false);
    sensor(to, true);
    tick(ASSISTANT_SETTLE_MS);
}

static void test_setup(void)
{
    now = 0;
    memset(sensors, 0, sizeof sensors);
    assistant_init(&game, now);
    sensor(SQ('a', 1), true);
    assert(assistant_light(&game, SQ('a', 1)) == LIGHT_GREEN);
    tick(ASSISTANT_GREEN_MS);
    assert(assistant_light(&game, SQ('a', 1)) == LIGHT_OFF);
    sensor(SQ('a', 1), false);
    sensor(SQ('a', 1), true);
    assert(assistant_light(&game, SQ('a', 1)) == LIGHT_GREEN);
    for (uint8_t s = 0; s < 64; ++s) sensors[s] = game.board[s] != EMPTY;
    sensors[SQ('e', 4)] = true;
    tick(40);
    tick(1000);
    assert(!game.playing);
    assert(assistant_light(&game, SQ('e', 4)) == LIGHT_RED);
    sensor(SQ('e', 4), false);
    tick(ASSISTANT_SETUP_MS - 1);
    assert(!game.playing);
    tick(1);
    assert(game.playing);
}

static void test_moves_and_cancel(void)
{
    start();
    sensor(SQ('e', 2), false);
    assert(game.selected == SQ('e', 2));
    assert(assistant_light(&game, SQ('e', 3)) == LIGHT_BLUE);
    assert(assistant_light(&game, SQ('e', 4)) == LIGHT_BLUE);
    assert(game.hints[SQ('e', 5)] == 0);
    tick(5000); /* No timeout on thinking with a piece in hand. */
    assert(game.board[SQ('e', 2)] == PAWN && game.white_turn);
    sensor(SQ('e', 2), true);
    assert(game.selected == NO_SQUARE && game.white_turn);
    move(SQ('e', 2), SQ('e', 4));
    assert(game.board[SQ('e', 2)] == EMPTY);
    assert(game.board[SQ('e', 4)] == PAWN && !game.white_turn);
    assert(game.warning_remaining == 0);
    move(SQ('e', 7), SQ('e', 5));
    assert(game.white_turn && game.board[SQ('e', 5)] == -PAWN);
}

static void test_capture(bool victim_first, bool same_snapshot)
{
    int8_t board[64] = {0};
    board[SQ('e', 1)] = KING;
    board[SQ('e', 8)] = -KING;
    board[SQ('e', 4)] = PAWN;
    board[SQ('d', 5)] = -PAWN;
    position(board, true);
    uint8_t from = SQ('e', 4), to = SQ('d', 5);
    if (same_snapshot) {
        sensors[from] = sensors[to] = false;
        tick(40);
    } else {
        sensor(victim_first ? to : from, false);
        tick(1000);
        assert(game.white_turn);
        sensor(victim_first ? from : to, false);
    }
    assert(assistant_light(&game, to) == LIGHT_ORANGE);
    tick(1000);
    assert(game.board[from] == PAWN && game.board[to] == -PAWN);
    sensor(to, true);
    tick(ASSISTANT_SETTLE_MS);
    assert(game.board[from] == EMPTY && game.board[to] == PAWN);
    assert(!game.white_turn && game.warning_remaining == 0);
}

static void test_capture_cancel(void)
{
    start();
    sensor(SQ('e', 7), false); /* Captured piece first, then change of mind. */
    sensor(SQ('e', 7), true);
    assert(game.white_turn && !game.removed[SQ('e', 7)]);
    sensor(SQ('e', 2), false);
    sensor(SQ('e', 7), false);
    sensors[SQ('e', 2)] = sensors[SQ('e', 7)] = true;
    tick(40);
    assert(game.white_turn && game.selected == NO_SQUARE);
    assert(game.board[SQ('e', 2)] == PAWN && game.board[SQ('e', 7)] == -PAWN);
}

static void test_advisory(void)
{
    start();
    move(SQ('e', 2), SQ('e', 5));
    assert(game.board[SQ('e', 5)] == PAWN && !game.white_turn);
    assert(assistant_light(&game, SQ('e', 5)) == LIGHT_RED);
    tick(220);
    assert(assistant_light(&game, SQ('e', 5)) != LIGHT_RED);
    move(SQ('d', 7), SQ('d', 5)); /* Play continues during the warning. */
    assert(game.white_turn && game.board[SQ('d', 5)] == -PAWN);
    tick(ASSISTANT_WARNING_MS);
    assert(game.warning_remaining == 0);
    move(SQ('a', 7), SQ('a', 6)); /* Wrong side is advisory too. */
    assert(game.white_turn && game.warning_remaining != 0);
    assert(game.board[SQ('a', 6)] == -PAWN);
}

static void test_settling_and_ambiguity(void)
{
    start();
    sensor(SQ('e', 2), false);
    sensor(SQ('e', 4), true);
    tick(ASSISTANT_SETTLE_MS - 1);
    assert(game.white_turn);
    sensor(SQ('e', 4), false); /* A brief landing is not a committed move. */
    sensor(SQ('e', 2), true);
    tick(1000);
    assert(game.white_turn && game.board[SQ('e', 2)] == PAWN);

    sensor(SQ('e', 2), false);
    sensor(SQ('d', 2), false);
    sensor(SQ('e', 4), true);
    tick(1000);
    assert(game.unresolved && game.white_turn);
    assert(game.board[SQ('e', 2)] == PAWN && game.board[SQ('d', 2)] == PAWN);
    sensors[SQ('e', 4)] = false;
    sensors[SQ('d', 2)] = sensors[SQ('e', 2)] = true;
    tick(40);
    assert(!game.unresolved);
    move(SQ('g', 1), SQ('f', 3));
    assert(game.board[SQ('f', 3)] == KNIGHT && !game.white_turn);
}

static void test_rules(void)
{
    int8_t board[64] = {0};
    board[SQ('e', 1)] = KING;
    board[SQ('a', 8)] = -KING;
    board[SQ('e', 2)] = ROOK;
    board[SQ('e', 8)] = -ROOK;
    assert(!chess_legal(board, SQ('e', 2), SQ('d', 2))); /* Pinned rook. */
    assert(chess_legal(board, SQ('e', 2), SQ('e', 8)));
    board[SQ('e', 2)] = EMPTY;
    position(board, true);
    assert(chess_in_check(board, true));
    assert(assistant_light(&game, SQ('e', 1)) == LIGHT_ORANGE);
    assert(!chess_legal(board, SQ('e', 1), SQ('e', 2)));
    assert(chess_legal(board, SQ('e', 1), SQ('d', 1)));
    board[SQ('e', 8)] = EMPTY;
    board[SQ('d', 3)] = -PAWN;
    assert(!chess_legal(board, SQ('e', 1), SQ('e', 2))); /* Pawn attack. */
    board[SQ('d', 3)] = EMPTY;
    board[SQ('f', 3)] = -KING;
    assert(!chess_legal(board, SQ('e', 1), SQ('e', 2))); /* Adjacent kings. */
    board[SQ('f', 3)] = EMPTY;
    board[SQ('b', 7)] = PAWN;
    assert(chess_legal(board, SQ('b', 7), SQ('b', 8)));
    chess_move(board, SQ('b', 7), SQ('b', 8));
    assert(board[SQ('b', 8)] == QUEEN);
    assert(!chess_legal(board, SQ('b', 8), SQ('a', 8))); /* No king capture hint. */
    assert(!chess_legal(board, 64, 0));
    board[SQ('c', 2)] = -PAWN;
    chess_move(board, SQ('c', 2), SQ('c', 1));
    assert(board[SQ('c', 1)] == -QUEEN);
}

static void ep_position(bool white, bool eligible)
{
    int8_t board[64] = {0};
    board[SQ('e', 1)] = KING;
    board[SQ('e', 8)] = -KING;
    board[SQ('e', white ? 5 : 4)] = white ? PAWN : -PAWN;
    board[SQ('d', white ? 5 : 4)] = white ? -PAWN : PAWN;
    position(board, white);
    game.en_passant = eligible ? SQ('d', white ? 6 : 3) : CHESS_NO_SQUARE;
}

static void test_en_passant_orders(void)
{
    static const uint8_t orders[6][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2},
        {1, 2, 0}, {2, 0, 1}, {2, 1, 0}
    };
    for (unsigned color = 0; color < 2; ++color) {
        bool white = color == 0;
        uint8_t from = SQ('e', white ? 5 : 4);
        uint8_t victim = SQ('d', white ? 5 : 4);
        uint8_t to = SQ('d', white ? 6 : 3);
        uint8_t squares[3] = {from, victim, to};
        for (unsigned eligible = 0; eligible < 2; ++eligible) {
            for (unsigned order = 0; order < 6; ++order) {
                ep_position(white, eligible != 0);
                for (unsigned event = 0; event < 3; ++event) {
                    uint8_t action = orders[order][event];
                    sensor(squares[action], action == 2);
                    tick(5000); /* Arbitrary pauses between all three events. */
                    if (event < 2) {
                        assert(game.white_turn == white);
                        assert(game.board[from] == (white ? PAWN : -PAWN));
                        assert(game.board[victim] == (white ? -PAWN : PAWN));
                    }
                }
                assert(game.board[from] == EMPTY && game.board[victim] == EMPTY);
                assert(game.board[to] == (white ? PAWN : -PAWN));
                assert(game.white_turn != white);
                assert((game.warning_remaining == 0) == (eligible != 0));
                assert(game.en_passant == CHESS_NO_SQUARE);
            }
        }
    }
    ep_position(true, true);
    sensors[SQ('e', 5)] = sensors[SQ('d', 5)] = false;
    sensors[SQ('d', 6)] = true;
    tick(40);
    tick(ASSISTANT_SETTLE_MS);
    assert(game.board[SQ('d', 6)] == PAWN && !game.white_turn);
}

static void test_en_passant_history_and_cancel(void)
{
    start();
    move(SQ('e', 2), SQ('e', 4));
    assert(game.en_passant == SQ('e', 3));
    move(SQ('a', 7), SQ('a', 6));
    assert(game.en_passant == CHESS_NO_SQUARE);
    move(SQ('e', 4), SQ('e', 5));
    move(SQ('d', 7), SQ('d', 5));
    assert(game.en_passant == SQ('d', 6));
    sensor(SQ('e', 5), false);
    assert(game.hints[SQ('d', 6)] == LIGHT_ORANGE);
    sensor(SQ('d', 6), true);
    tick(5000);
    assert(game.en_passant_pending && game.white_turn);
    assert(strcmp(assistant_status(&game), "Finish en passant") == 0);
    sensor(SQ('d', 6), false);
    sensor(SQ('e', 5), true);
    assert(!game.en_passant_pending && game.white_turn);
    assert(game.en_passant == SQ('d', 6));
    move(SQ('g', 1), SQ('f', 3));
    assert(game.en_passant == CHESS_NO_SQUARE);

    ep_position(true, true);
    sensor(SQ('e', 5), false);
    sensor(SQ('d', 5), false);
    sensors[SQ('e', 5)] = sensors[SQ('d', 5)] = true;
    tick(40);
    assert(game.white_turn && game.en_passant == SQ('d', 6));

    ep_position(true, true);
    game.white_turn = false;
    sensor(SQ('e', 5), false);
    sensor(SQ('d', 5), false);
    sensor(SQ('d', 6), true);
    tick(ASSISTANT_SETTLE_MS);
    assert(!game.white_turn && game.warning_remaining != 0);

    start();
    move(SQ('d', 7), SQ('d', 5)); /* Wrong-turn double move grants no right. */
    assert(game.en_passant == CHESS_NO_SQUARE);

    /* Unrelated changes must not be swallowed as part of en passant. */
    ep_position(true, true);
    sensor(SQ('e', 5), false);
    sensor(SQ('d', 5), false);
    sensor(SQ('d', 6), true);
    sensor(SQ('a', 3), true);
    tick(5000);
    assert(game.unresolved && game.white_turn);
}

static void test_en_passant_rules(void)
{
    ep_position(true, true);
    uint8_t from = SQ('e', 5), to = SQ('d', 6);
    assert(chess_legal_en_passant(game.board, from, to, to));
    assert(!chess_legal_en_passant(game.board, from, to, CHESS_NO_SQUARE));
    game.en_passant = CHESS_NO_SQUARE;
    sensor(from, false);
    assert(game.hints[to] == 0);
    sensor(from, true);
    game.en_passant = to;
    assert(chess_en_passant_victim(game.board, 64, to) == CHESS_NO_SQUARE);
    assert(chess_en_passant_victim(game.board, from, 64) == CHESS_NO_SQUARE);
    game.board[SQ('e', 8)] = -ROOK;
    assert(!chess_legal_en_passant(game.board, from, to, to));
    sensor(from, false);
    assert(game.hints[to] == 0); /* Pinned pawn must not receive a hint. */
    sensor(to, true);
    tick(5000);
    assert(game.white_turn); /* Still wait for the victim of an illegal move. */
    sensor(SQ('d', 5), false);
    tick(ASSISTANT_SETTLE_MS);
    assert(!game.white_turn && game.warning_remaining != 0);

    /* Removing both pawns can expose a rook along the fifth rank. */
    ep_position(true, true);
    game.board[SQ('e', 1)] = EMPTY;
    game.board[SQ('h', 5)] = KING;
    game.board[SQ('a', 5)] = -ROOK;
    assert(!chess_legal_en_passant(game.board, from, to, to));
    game.board[SQ('a', 5)] = EMPTY;
    assert(chess_legal_en_passant(game.board, from, to, to));
    game.board[SQ('d', 5)] = -KNIGHT;
    assert(chess_en_passant_victim(game.board, from, to) == CHESS_NO_SQUARE);
    ep_position(false, true);
    assert(chess_legal_en_passant(game.board, SQ('e', 4), SQ('d', 3), SQ('d', 3)));
}

static unsigned long perft(const int8_t board[64], bool white, unsigned depth)
{
    if (depth == 0) return 1;
    unsigned long count = 0;
    for (uint8_t from = 0; from < 64; ++from) {
        if (board[from] == EMPTY || (board[from] > 0) != white) continue;
        for (uint8_t to = 0; to < 64; ++to) {
            if (!chess_legal(board, from, to)) continue;
            int8_t next[64];
            memcpy(next, board, 64);
            chess_move(next, from, to);
            count += perft(next, !white, depth - 1);
        }
    }
    return count;
}

static void test_clock_wrap(void)
{
    start();
    now = UINT32_MAX - 100U;
    game.last_update = now;
    move(SQ('e', 2), SQ('e', 5));
    assert(!game.white_turn && game.warning_remaining == ASSISTANT_WARNING_MS);
    tick(ASSISTANT_WARNING_MS);
    assert(game.warning_remaining == 0);
}

int main(void)
{
    test_setup();
    test_moves_and_cancel();
    test_capture(false, false);
    test_capture(true, false);
    test_capture(false, true);
    test_capture_cancel();
    test_advisory();
    test_settling_and_ambiguity();
    test_rules();
    test_clock_wrap();
    test_en_passant_orders();
    test_en_passant_history_and_cancel();
    test_en_passant_rules();
    int8_t board[64];
    chess_start(board);
    assert(perft(board, true, 1) == 20);
    assert(perft(board, true, 2) == 400);
    assert(perft(board, true, 3) == 8902);
    puts("All assistant and chess tests passed (perft: 20 / 400 / 8902).");
    return 0;
}
