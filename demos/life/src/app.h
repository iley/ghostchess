#ifndef APP_H
#define APP_H

#include "life.h"

#define APP_COLUMNS 16U
#define APP_BUTTON_DEBOUNCE_MS 50U
#define APP_REPEAT_DELAY_MS 400U
#define APP_REPEAT_INTERVAL_MS 120U
/* How long a dead, still, or looping board keeps playing before a new seed. */
#define APP_RESEED_MS 5000U
#define APP_SPEEDS 3U
/* Longest cell fade; faster speeds shorten it to one step. */
#define APP_FADE_MS 500U

extern const uint16_t app_step_ms[APP_SPEEDS];

/*
 * Playing:  BTN1 new seed, BTN2 pause, BTN3 cycle speed.
 * Paused:   BTN1 step back, BTN2 play, BTN3 step forward; holding a step
 *           button repeats it.
 */
struct app {
    struct life life;
    /* The grid shown before the latest change, for fading between the two. */
    uint8_t from[LIFE_SIZE];
    uint32_t grid_changed_at;
    uint32_t rng;
    bool paused;
    uint8_t speed;
    uint32_t stepped_at;
    bool settled;
    uint32_t settled_since;
    uint8_t candidate;
    uint8_t down;
    uint32_t changed_at[3];
    uint32_t repeat_from;
    uint16_t repeat_wait;
};

/* The seed only needs to differ between boots; button timing adds entropy. */
void app_init(struct app *app, uint32_t seed, uint32_t now);

/* Bits 0..2 of pressed are the raw (undebounced) BTN1..BTN3 states. */
void app_update(struct app *app, uint8_t pressed, uint32_t now);

/* Progress from `from` to the current grid: 0 at the change, 255 when done. */
uint8_t app_fade(const struct app *app, uint32_t now);

/* Fill both display lines, each APP_COLUMNS characters plus a terminator. */
void app_text(const struct app *app, char top[APP_COLUMNS + 1U],
              char bottom[APP_COLUMNS + 1U]);

#endif
