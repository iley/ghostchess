#include "chess.h"

#include <string.h>

static int magnitude(int value) { return value < 0 ? -value : value; }

void chess_start(int8_t board[64])
{
    static const int8_t back[8] = { ROOK, KNIGHT, BISHOP, QUEEN,
                                   KING, BISHOP, KNIGHT, ROOK };
    memset(board, 0, 64);
    for (uint8_t f = 0; f < 8; ++f) {
        board[f] = -back[f];
        board[8 + f] = -PAWN;
        board[48 + f] = PAWN;
        board[56 + f] = back[f];
    }
}

static bool reaches(const int8_t board[64], uint8_t from, uint8_t to,
                    bool attack)
{
    int piece = board[from];
    int dr = (int)(to / 8) - (int)(from / 8);
    int df = (int)(to % 8) - (int)(from % 8);
    int ar = magnitude(dr), af = magnitude(df);
    if (from == to || piece == EMPTY) return false;

    switch (magnitude(piece)) {
    case PAWN: {
        int step = piece > 0 ? -1 : 1;
        if (attack) return dr == step && af == 1;
        if (af == 1) return dr == step && board[to] != EMPTY;
        if (df != 0 || board[to] != EMPTY) return false;
        if (dr == step) return true;
        return dr == 2 * step && from / 8 == (piece > 0 ? 6 : 1) &&
               board[(int)from + step * 8] == EMPTY;
    }
    case KNIGHT: return (ar == 2 && af == 1) || (ar == 1 && af == 2);
    case KING: return ar <= 1 && af <= 1;
    case BISHOP: if (ar != af) return false; break;
    case ROOK: if (dr != 0 && df != 0) return false; break;
    case QUEEN: if (dr != 0 && df != 0 && ar != af) return false; break;
    default: return false;
    }

    int step = (dr == 0 ? 0 : (dr > 0 ? 8 : -8)) +
               (df == 0 ? 0 : (df > 0 ? 1 : -1));
    for (int square = (int)from + step; square != to; square += step) {
        if (board[square] != EMPTY) return false;
    }
    return true;
}

bool chess_in_check(const int8_t board[64], bool white)
{
    for (uint8_t king = 0; king < 64; ++king) {
        if (board[king] != (white ? KING : -KING)) continue;
        for (uint8_t enemy = 0; enemy < 64; ++enemy) {
            if (board[enemy] != EMPTY && (board[enemy] > 0) != white &&
                reaches(board, enemy, king, true)) return true;
        }
    }
    return false;
}

void chess_move(int8_t board[64], uint8_t from, uint8_t to)
{
    int8_t piece = board[from];
    board[from] = EMPTY;
    if (piece == PAWN && to / 8 == 0) piece = QUEEN;
    if (piece == -PAWN && to / 8 == 7) piece = -QUEEN;
    board[to] = piece;
}

bool chess_legal(const int8_t board[64], uint8_t from, uint8_t to)
{
    if (from >= 64 || to >= 64 || from == to || board[from] == EMPTY)
        return false;
    if (board[to] != EMPTY && (board[from] > 0) == (board[to] > 0))
        return false;
    if (magnitude(board[to]) == KING || !reaches(board, from, to, false))
        return false;

    int8_t next[64];
    memcpy(next, board, sizeof next);
    chess_move(next, from, to);
    return !chess_in_check(next, board[from] > 0);
}

uint8_t chess_en_passant_victim(const int8_t board[64], uint8_t from, uint8_t to)
{
    if (from >= 64 || to >= 64 || board[to] != EMPTY) return CHESS_NO_SQUARE;
    int piece = board[from];
    if (magnitude(piece) != PAWN || from / 8 != (piece > 0 ? 3 : 4))
        return CHESS_NO_SQUARE;
    int step = piece > 0 ? -1 : 1;
    if ((int)(to / 8) - from / 8 != step ||
        magnitude((int)(to % 8) - from % 8) != 1) return CHESS_NO_SQUARE;
    uint8_t victim = (uint8_t)((int)to - step * 8);
    return board[victim] == -piece ? victim : CHESS_NO_SQUARE;
}

bool chess_legal_en_passant(const int8_t board[64], uint8_t from, uint8_t to,
                          uint8_t eligible)
{
    uint8_t victim = chess_en_passant_victim(board, from, to);
    if (to != eligible || victim == CHESS_NO_SQUARE) return false;
    int8_t next[64];
    memcpy(next, board, sizeof next);
    next[victim] = EMPTY;
    chess_move(next, from, to);
    return !chess_in_check(next, board[from] > 0);
}

uint8_t chess_en_passant_target(const int8_t board[64], uint8_t from, uint8_t to)
{
    if (from >= 64 || to >= 64 || magnitude(board[from]) != PAWN ||
        magnitude((int)to - from) != 16 || !chess_legal(board, from, to))
        return CHESS_NO_SQUARE;
    return (uint8_t)((from + to) / 2);
}
