#include "app.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct life life;
static struct app app;
static uint32_t now;

static const uint8_t BLINKER[LIFE_SIZE] = { 0, 0, 0x1c, 0, 0, 0, 0, 0 };
static const uint8_t BLOCK[LIFE_SIZE] = { 0, 0x06, 0x06, 0, 0, 0, 0, 0 };
static const uint8_t GLIDER[LIFE_SIZE] = { 0x02, 0x04, 0x07, 0, 0, 0, 0, 0 };

static void test_rules(void)
{
    life_start(&life, BLINKER);
    assert(life_population(&life) == 3 && life.period == 0);
    assert(life_previous(&life) == NULL);
    life_step(&life);
    static const uint8_t vertical[LIFE_SIZE] = { 0, 0x08, 0x08, 0x08, 0, 0, 0, 0 };
    assert(memcmp(life_current(&life), vertical, LIFE_SIZE) == 0);
    assert(memcmp(life_previous(&life), BLINKER, LIFE_SIZE) == 0);
    life_step(&life);
    assert(life.period == 2);

    life_start(&life, BLOCK);
    life_step(&life);
    assert(life.period == 1 && life_population(&life) == 4);
}

static void test_wrapping(void)
{
    /* A blinker across the side edge turns into one across the top edge. */
    static const uint8_t side[LIFE_SIZE] = { 0x83, 0, 0, 0, 0, 0, 0, 0 };
    static const uint8_t top[LIFE_SIZE] = { 0x01, 0x01, 0, 0, 0, 0, 0, 0x01 };
    life_start(&life, side);
    life_step(&life);
    assert(memcmp(life_current(&life), top, LIFE_SIZE) == 0);
    life_step(&life);
    assert(memcmp(life_current(&life), side, LIFE_SIZE) == 0);

    /* A glider crosses the 8x8 torus diagonally in 32 generations. */
    life_start(&life, GLIDER);
    for (int i = 0; i < 31; ++i) {
        life_step(&life);
        assert(life.period == 0);
    }
    life_step(&life);
    assert(life.period == 32);
    assert(memcmp(life_current(&life), GLIDER, LIFE_SIZE) == 0);
}

static void test_history(void)
{
    life_start(&life, GLIDER);
    for (int i = 0; i < 100; ++i) life_step(&life);
    for (unsigned i = 0; i < LIFE_HISTORY - 1U; ++i) {
        assert(life_can_step_back(&life));
        life_step_back(&life);
    }
    assert(life.generation == 100 - (LIFE_HISTORY - 1U));
    assert(!life_can_step_back(&life) && life_previous(&life) == NULL);
    life_step_back(&life);
    assert(life.generation == 100 - (LIFE_HISTORY - 1U));

    /* Stepping forward again reproduces the same generations. The glider
     * returns to its start every 32 generations; generation 64 is the oldest
     * stored match for generation 96. */
    while (life.generation != 96U) life_step(&life);
    assert(memcmp(life_current(&life), GLIDER, LIFE_SIZE) == 0);
    assert(life.period == 32);

    /* Going back to generation 0 restores a non-repeating state. */
    life_start(&life, GLIDER);
    life_step(&life);
    life_step_back(&life);
    assert(life.generation == 0 && life.period == 0);
    assert(memcmp(life_current(&life), GLIDER, LIFE_SIZE) == 0);
}

static void test_seed(void)
{
    uint32_t rng = 0;
    unsigned total = 0;
    for (int i = 0; i < 100; ++i) {
        life_seed(&life, &rng);
        assert(life.generation == 0 && life.period == 0);
        total += life_population(&life);
    }
    /* About 3/8 of 64 cells. */
    assert(total > 2100 && total < 2700);

    uint32_t a = 42, b = 42;
    struct life other;
    life_seed(&life, &a);
    life_seed(&other, &b);
    assert(memcmp(life_current(&life), life_current(&other), LIFE_SIZE) == 0);
}

static void tick(uint8_t pressed, uint32_t ms)
{
    for (uint32_t end = now + ms; now < end; now += 10) app_update(&app, pressed, now);
}

static void click(uint8_t button)
{
    tick(button, 60);
    tick(0, 60);
}

static void text(char top[17], char bottom[17])
{
    app_text(&app, top, bottom);
    assert(strlen(top) == APP_COLUMNS && strlen(bottom) == APP_COLUMNS);
}

static void start_app(const uint8_t grid[LIFE_SIZE])
{
    now = 1000;
    app_init(&app, 7, now);
    life_start(&app.life, grid);
}

static void test_play_and_pause(void)
{
    char top[17], bottom[17];
    start_app(GLIDER);
    text(top, bottom);
    assert(strcmp(top, "Gen 0      Pop 5") == 0);
    assert(strcmp(bottom, " New Pause  >>  ") == 0);

    tick(0, app_step_ms[1] * 3 + 5);
    assert(app.life.generation == 3);

    click(2); /* pause */
    assert(app.paused);
    uint32_t paused_at = app.life.generation;
    tick(0, 3000);
    assert(app.life.generation == paused_at);
    text(top, bottom);
    assert(strcmp(bottom, "<Back Play Step>") == 0);

    click(4);
    assert(app.life.generation == paused_at + 1);
    click(1);
    click(1);
    assert(app.life.generation == paused_at - 1);

    /* Holding Step repeats after a delay. */
    tick(4, APP_BUTTON_DEBOUNCE_MS + APP_REPEAT_DELAY_MS + 3 * APP_REPEAT_INTERVAL_MS + 20);
    tick(0, 100);
    assert(app.life.generation == paused_at - 1 + 1 + 1 + 3);

    /* No back label once the start is reached. */
    while (life_can_step_back(&app.life)) click(1);
    assert(app.life.generation == 0);
    text(top, bottom);
    assert(strcmp(bottom, "      Play Step>") == 0);

    click(2); /* play */
    assert(!app.paused);
    click(4); /* speed */
    assert(app.speed == 2);
    click(4);
    assert(app.speed == 0);
}

static void test_bounce_and_new_seed(void)
{
    start_app(GLIDER);
    /* A press shorter than the debounce time is ignored. */
    tick(1, 30);
    tick(0, 100);
    assert(memcmp(life_current(&app.life), GLIDER, LIFE_SIZE) == 0);

    click(1);
    assert(app.life.generation == 0);
    assert(memcmp(life_current(&app.life), GLIDER, LIFE_SIZE) != 0);
}

static void test_end_states(void)
{
    char top[17], bottom[17];
    start_app(BLOCK);
    tick(0, app_step_ms[1] + 5);
    text(top, bottom);
    assert(strcmp(top, "Gen 1      Still") == 0);
    /* The finished board stays visible, then a new seed starts. */
    tick(0, APP_RESEED_MS - 100);
    assert(app.life.generation > 1);
    tick(0, 200);
    assert(app.life.generation <= 1);

    static const uint8_t empty[LIFE_SIZE] = { 0 };
    start_app(empty);
    text(top, bottom);
    assert(strcmp(top, "Gen 0       Dead") == 0);

    start_app(BLINKER);
    click(2);
    click(4);
    click(4);
    text(top, bottom);
    assert(strcmp(top, "Gen 2      Loop2") == 0);

    /* The longest generation that still leaves room for any state. */
    start_app(BLINKER);
    app.life.generation = app.life.newest = 99999;
    text(top, bottom);
    assert(strncmp(top, "Gen 99999 ", 10) == 0);
}

static void test_fade(void)
{
    start_app(BLINKER);
    click(2); /* pause */
    tick(0, APP_FADE_MS);
    assert(app_fade(&app, now) == 255);

    /* A manual step fades from the grid that was shown. */
    tick(4, APP_BUTTON_DEBOUNCE_MS + 10);
    assert(memcmp(app.from, BLINKER, LIFE_SIZE) == 0);
    assert(app_fade(&app, now) < 20);
    tick(0, APP_FADE_MS / 2);
    assert(app_fade(&app, now) > 100 && app_fade(&app, now) < 160);
    tick(0, APP_FADE_MS / 2);
    assert(app_fade(&app, now) == 255);

    /* Stepping back at the history limit changes nothing, so starts no fade. */
    while (life_can_step_back(&app.life)) click(1);
    tick(0, APP_FADE_MS);
    click(1);
    assert(app_fade(&app, now) == 255);

    /* At the fastest speed a fade fits inside one step. */
    click(2); /* play */
    click(4);
    assert(app.speed == 2 && app_step_ms[2] < APP_FADE_MS);
    assert(app_fade(&app, app.grid_changed_at + app_step_ms[2]) == 255);
    assert(app_fade(&app, app.grid_changed_at + app_step_ms[2] - 10) < 255);
}

int main(void)
{
    test_rules();
    test_wrapping();
    test_history();
    test_seed();
    test_play_and_pause();
    test_bounce_and_new_seed();
    test_end_states();
    test_fade();
    puts("life tests passed");
    return 0;
}
