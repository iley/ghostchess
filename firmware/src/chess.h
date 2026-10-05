#ifndef CHESS_H
#define CHESS_H

#include <stdbool.h>
#include <stdint.h>

/* A8 = 0, H8 = 7, A1 = 56. Positive pieces are white. */
enum chess_piece { EMPTY, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

void chess_start(int8_t board[64]);
bool chess_in_check(const int8_t board[64], bool white);
/* Ordinary moves, including automatic queen promotion; no castle/en passant. */
bool chess_legal(const int8_t board[64], uint8_t from, uint8_t to);
void chess_move(int8_t board[64], uint8_t from, uint8_t to);

#endif
