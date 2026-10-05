#ifndef CHESS_H
#define CHESS_H

#include <stdbool.h>
#include <stdint.h>

/* A8 = 0, H8 = 7, A1 = 56. Positive pieces are white. */
enum chess_piece { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };
#define CHESS_NO_SQUARE 64U

#define CHESS_CASTLE_ALL 15U
/* Bits: white kingside, white queenside, black kingside, black queenside. */
uint8_t chess_castle_right(bool white, bool kingside);
bool chess_castle_shape(const int8_t board[64], bool white, bool kingside);
bool chess_legal_castle(const int8_t board[64], bool white, bool kingside,
                        uint8_t rights);
uint8_t chess_rights_after(const int8_t board[64], uint8_t from, uint8_t to,
                           uint8_t rights);

void chess_start(int8_t board[64]);
bool chess_in_check(const int8_t board[64], bool white);
/* Ordinary moves, including automatic queen promotion; no castle/en passant. */
bool chess_legal(const int8_t board[64], uint8_t from, uint8_t to);
void chess_move(int8_t board[64], uint8_t from, uint8_t to);
/* Geometry only: also identifies advisory-invalid, expired en passant. */
uint8_t chess_en_passant_victim(const int8_t board[64], uint8_t from, uint8_t to);
bool chess_legal_en_passant(const int8_t board[64], uint8_t from, uint8_t to,
                          uint8_t eligible);
/* Eligibility for the reply to an ordinary move, or CHESS_NO_SQUARE. */
uint8_t chess_en_passant_target(const int8_t board[64], uint8_t from, uint8_t to);

#endif
