#include "life.h"

#include <string.h>

static uint8_t neighbours(const uint8_t *grid, uint8_t row, uint8_t file);
static bool alive(const uint8_t *grid, uint8_t row, uint8_t file);
static void find_period(struct life *life);
static uint32_t oldest_stored(const struct life *life);

void life_seed(struct life *life, uint32_t *rng)
{
    uint8_t grid[LIFE_SIZE];
    for (uint8_t row = 0; row < LIFE_SIZE; ++row) {
        grid[row] = 0;
        for (uint8_t file = 0; file < LIFE_SIZE; ++file) {
            /* The top bits of xorshift32 are better distributed. */
            if ((life_random(rng) >> 29) < 3U) grid[row] |= (uint8_t)(1U << file);
        }
    }
    life_start(life, grid);
}

void life_start(struct life *life, const uint8_t grid[LIFE_SIZE])
{
    memcpy(life->history[0], grid, LIFE_SIZE);
    life->generation = 0;
    life->newest = 0;
    life->period = 0;
}

void life_step(struct life *life)
{
    const uint8_t *now = life_current(life);
    uint8_t *next = life->history[(life->generation + 1U) % LIFE_HISTORY];
    for (uint8_t row = 0; row < LIFE_SIZE; ++row) {
        uint8_t bits = 0;
        for (uint8_t file = 0; file < LIFE_SIZE; ++file) {
            uint8_t count = neighbours(now, row, file);
            if (count == 3U || (count == 2U && alive(now, row, file)))
                bits |= (uint8_t)(1U << file);
        }
        next[row] = bits;
    }
    ++life->generation;
    if (life->generation > life->newest) life->newest = life->generation;
    find_period(life);
}

static uint8_t neighbours(const uint8_t *grid, uint8_t row, uint8_t file)
{
    uint8_t count = 0;
    for (uint8_t dr = 0; dr < 3U; ++dr) {
        for (uint8_t df = 0; df < 3U; ++df) {
            if (dr == 1U && df == 1U) continue;
            /* Adding LIFE_SIZE - 1 instead of subtracting 1 keeps it unsigned. */
            uint8_t r = (uint8_t)((row + dr + LIFE_SIZE - 1U) % LIFE_SIZE);
            uint8_t f = (uint8_t)((file + df + LIFE_SIZE - 1U) % LIFE_SIZE);
            count += alive(grid, r, f);
        }
    }
    return count;
}

static bool alive(const uint8_t *grid, uint8_t row, uint8_t file)
{
    return ((grid[row] >> file) & 1U) != 0U;
}

static void find_period(struct life *life)
{
    const uint8_t *now = life_current(life);
    uint32_t oldest = oldest_stored(life);
    life->period = 0;
    for (uint32_t g = life->generation; g-- > oldest;) {
        if (memcmp(life->history[g % LIFE_HISTORY], now, LIFE_SIZE) == 0) {
            life->period = (uint8_t)(life->generation - g);
            return;
        }
    }
}

static uint32_t oldest_stored(const struct life *life)
{
    return life->newest >= LIFE_HISTORY ? life->newest - LIFE_HISTORY + 1U : 0U;
}

bool life_can_step_back(const struct life *life)
{
    return life->generation > oldest_stored(life);
}

void life_step_back(struct life *life)
{
    if (!life_can_step_back(life)) return;
    --life->generation;
    find_period(life);
}

const uint8_t *life_current(const struct life *life)
{
    return life->history[life->generation % LIFE_HISTORY];
}

const uint8_t *life_previous(const struct life *life)
{
    if (!life_can_step_back(life)) return NULL;
    return life->history[(life->generation - 1U) % LIFE_HISTORY];
}

uint8_t life_population(const struct life *life)
{
    const uint8_t *grid = life_current(life);
    uint8_t count = 0;
    for (uint8_t row = 0; row < LIFE_SIZE; ++row) {
        for (uint8_t bits = grid[row]; bits != 0U; bits &= (uint8_t)(bits - 1U)) ++count;
    }
    return count;
}

uint32_t life_random(uint32_t *state)
{
    uint32_t x = *state != 0U ? *state : 0x9e3779b9U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}
