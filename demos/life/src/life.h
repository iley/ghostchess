#ifndef LIFE_H
#define LIFE_H

#include <stdbool.h>
#include <stdint.h>

#define LIFE_SIZE 8U

/* Generations kept for stepping back and loop detection. 64 covers a glider's
 * 32-generation trip around the 8x8 torus. */
#define LIFE_HISTORY 64U

/*
 * Conway's Game of Life on an 8x8 torus: edges wrap, so patterns on such a
 * small board live longer. Row 0 is the far rank; bit f of a row is file f.
 * Generation g is stored in history[g % LIFE_HISTORY].
 */
struct life {
    uint8_t history[LIFE_HISTORY][LIFE_SIZE];
    uint32_t generation;
    uint32_t newest; /* highest generation computed since the start */
    /* Generations since the current grid last occurred, or 0 if it does not
     * repeat a stored generation. A dead or still board has period 1. */
    uint8_t period;
};

/* Start from a random grid with roughly 3/8 of the cells alive. */
void life_seed(struct life *life, uint32_t *rng);

/* Start from the given grid as generation 0. */
void life_start(struct life *life, const uint8_t grid[LIFE_SIZE]);

void life_step(struct life *life);

/* Stepping back is limited to generations still held in the history. */
bool life_can_step_back(const struct life *life);
void life_step_back(struct life *life);

const uint8_t *life_current(const struct life *life);

/* The preceding generation, or NULL when it is not stored. */
const uint8_t *life_previous(const struct life *life);

uint8_t life_population(const struct life *life);

/* xorshift32; a zero state is replaced so the sequence never sticks. */
uint32_t life_random(uint32_t *state);

#endif
