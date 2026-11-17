#include "app.h"

#include <string.h>

const uint16_t app_step_ms[APP_SPEEDS] = { 1600U, 800U, 350U };

static const char *const speed_labels[APP_SPEEDS] = { ">", ">>", ">>>" };

static void new_seed(struct app *app, uint32_t now);
static void step_forward(struct app *app, uint32_t now);
static void step_back(struct app *app, uint32_t now);
static void mark_change(struct app *app, uint32_t now);
static uint8_t debounce(struct app *app, uint8_t pressed, uint32_t now);
static void press(struct app *app, uint8_t button, uint32_t now);
static void repeat(struct app *app, uint32_t now);
static void play(struct app *app, uint32_t now);
static void status(const struct life *life, char text[7]);
static void format_number(char *out, uint32_t value);
static void put(char *line, uint8_t column, const char *text);
static void put_centered(char *line, uint8_t column, uint8_t width, const char *text);

void app_init(struct app *app, uint32_t seed, uint32_t now)
{
    memset(app, 0, sizeof *app);
    app->rng = seed;
    app->speed = 1;
    new_seed(app, now);
}

static void new_seed(struct app *app, uint32_t now)
{
    mark_change(app, now);
    life_seed(&app->life, &app->rng);
    app->stepped_at = now;
    app->settled = false;
}

static void step_forward(struct app *app, uint32_t now)
{
    mark_change(app, now);
    life_step(&app->life);
}

static void step_back(struct app *app, uint32_t now)
{
    /* At the history limit nothing changes, so there is nothing to fade. */
    if (!life_can_step_back(&app->life)) return;
    mark_change(app, now);
    life_step_back(&app->life);
}

/* Call before changing the grid. */
static void mark_change(struct app *app, uint32_t now)
{
    memcpy(app->from, life_current(&app->life), LIFE_SIZE);
    app->grid_changed_at = now;
}

void app_update(struct app *app, uint8_t pressed, uint32_t now)
{
    uint8_t rising = debounce(app, pressed, now);
    for (uint8_t button = 0; button < 3U; ++button) {
        if (rising & (1U << button)) {
            /* Human press timing makes each seed after the first unpredictable. */
            app->rng ^= now;
            press(app, button, now);
        }
    }
    if (app->paused) {
        repeat(app, now);
    } else {
        play(app, now);
    }
}

/* Returns the buttons that became pressed after being stable for the debounce
 * time. */
static uint8_t debounce(struct app *app, uint8_t pressed, uint32_t now)
{
    uint8_t rising = 0;
    for (uint8_t i = 0; i < 3U; ++i) {
        uint8_t bit = (uint8_t)(1U << i);
        if ((pressed & bit) != (app->candidate & bit)) {
            app->candidate ^= bit;
            app->changed_at[i] = now;
        }
        if ((app->candidate & bit) != (app->down & bit) &&
            now - app->changed_at[i] >= APP_BUTTON_DEBOUNCE_MS) {
            app->down ^= bit;
            if (app->down & bit) rising |= bit;
        }
    }
    return rising;
}

static void press(struct app *app, uint8_t button, uint32_t now)
{
    if (app->paused) {
        switch (button) {
        case 0: step_back(app, now); break;
        case 1:
            app->paused = false;
            app->stepped_at = now;
            break;
        default: step_forward(app, now); break;
        }
        app->repeat_from = now;
        app->repeat_wait = APP_REPEAT_DELAY_MS;
    } else {
        switch (button) {
        case 0: new_seed(app, now); break;
        case 1:
            app->paused = true;
            app->settled = false;
            break;
        default: app->speed = (uint8_t)((app->speed + 1U) % APP_SPEEDS); break;
        }
    }
}

static void repeat(struct app *app, uint32_t now)
{
    /* Only a lone held step button repeats. */
    if (app->down != 1U && app->down != 4U) return;
    if (now - app->repeat_from < app->repeat_wait) return;
    if (app->down == 1U) {
        step_back(app, now);
    } else {
        step_forward(app, now);
    }
    app->repeat_from = now;
    app->repeat_wait = APP_REPEAT_INTERVAL_MS;
}

static void play(struct app *app, uint32_t now)
{
    if (now - app->stepped_at >= app_step_ms[app->speed]) {
        app->stepped_at = now;
        step_forward(app, now);
    }
    /* Let a finished pattern be seen for a while before replacing it. */
    if (app->life.period == 0U) {
        app->settled = false;
    } else if (!app->settled) {
        app->settled = true;
        app->settled_since = now;
    } else if (now - app->settled_since >= APP_RESEED_MS) {
        new_seed(app, now);
    }
}

uint8_t app_fade(const struct app *app, uint32_t now)
{
    uint32_t duration = APP_FADE_MS;
    if (!app->paused && app_step_ms[app->speed] < duration) duration = app_step_ms[app->speed];
    uint32_t elapsed = now - app->grid_changed_at;
    if (elapsed >= duration) return 255U;
    return (uint8_t)(elapsed * 255U / duration);
}

/*
 * Top:    "Gen 123   Pop 17"   generation, then population or end state
 * Bottom: " New Pause  >>  "   labels above BTN1, BTN2, BTN3
 */
void app_text(const struct app *app, char top[APP_COLUMNS + 1U],
              char bottom[APP_COLUMNS + 1U])
{
    char number[11];
    char state[7];
    memset(top, ' ', APP_COLUMNS);
    memset(bottom, ' ', APP_COLUMNS);
    top[APP_COLUMNS] = bottom[APP_COLUMNS] = '\0';

    put(top, 0, "Gen ");
    format_number(number, app->life.generation);
    put(top, 4, number);
    /* Written last so the state stays readable if the generation is long. */
    status(&app->life, state);
    put(top, (uint8_t)(APP_COLUMNS - strlen(state)), state);

    if (app->paused) {
        put_centered(bottom, 0, 5, life_can_step_back(&app->life) ? "<Back" : "");
        put_centered(bottom, 5, 6, "Play");
        put_centered(bottom, 11, 5, "Step>");
    } else {
        put_centered(bottom, 0, 5, "New");
        put_centered(bottom, 5, 6, "Pause");
        put_centered(bottom, 11, 5, speed_labels[app->speed]);
    }
}

/* At most 6 characters, so it fits beside a generation below 100000. */
static void status(const struct life *life, char text[7])
{
    uint8_t population = life_population(life);
    if (population == 0U) {
        strcpy(text, "Dead");
    } else if (life->period == 1U) {
        strcpy(text, "Still");
    } else if (life->period != 0U) {
        /* No space: periods reach two digits and "Loop63" must fit. */
        strcpy(text, "Loop");
        format_number(text + 4, life->period);
    } else {
        strcpy(text, "Pop ");
        format_number(text + 4, population);
    }
}

/* Writes up to 10 digits and a terminator. */
static void format_number(char *out, uint32_t value)
{
    char reversed[10];
    uint8_t length = 0;
    do {
        reversed[length++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    for (uint8_t i = 0; i < length; ++i) out[i] = reversed[length - 1U - i];
    out[length] = '\0';
}

static void put(char *line, uint8_t column, const char *text)
{
    while (*text != '\0' && column < APP_COLUMNS) line[column++] = *text++;
}

static void put_centered(char *line, uint8_t column, uint8_t width, const char *text)
{
    put(line, (uint8_t)(column + (width - strlen(text)) / 2U), text);
}
