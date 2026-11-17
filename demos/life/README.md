# Game of Life demo

Conway's Game of Life on the Ghost Chess board's 8×8 LEDs, for the
`ghostchess_brains_v1` controller. It is a standalone PlatformIO project; the
hardware code (LED driver, OLED driver, timer, build hook) is copied from
[`firmware/`](../../firmware), which documents the hardware, fuses, and the
Apple Silicon toolchain setup.

The board is a torus: cells on one edge neighbour the opposite edge, so
gliders travel around instead of dying at the border. Each start is a random
pattern with about 3/8 of the cells alive.

## Using it

The OLED's top line shows the generation and the population. When the board
dies, stops changing, or repeats an earlier generation, the population is
replaced by `Dead`, `Still`, or `LoopN` (repeats every N generations). The
bottom line labels the three buttons, BTN1 to BTN3 from left to right:

| Mode | BTN1 | BTN2 | BTN3 |
|---|---|---|---|
| Playing | `New` random pattern | `Pause` | speed: `>` `>>` `>>>` |
| Paused | `<Back` one generation | `Play` | `Step>` one generation |

Hold `<Back` or `Step>` to repeat. Going back is limited to the last 64
generations; the `<Back` label disappears at the limit. While playing, a
finished pattern stays visible for 5 seconds, then a new one starts.

Live cells are white. Newborn cells fade in from dim green to white, and dying
cells fade out to black. Fades last up to half a second and always finish
before the next step. The speeds step every 1.6 s, 0.8 s, and 0.35 s.

The first pattern differs on every power-up thanks to a counter in EEPROM;
the timing of button presses randomizes later patterns.

## Build and upload

```sh
cd demos/life
pio run --target upload
```

Flash the chess firmware again from `firmware/` to go back to chess.

## Host tests

```sh
sh demos/life/tests/run.sh  # from the repository root
```

The tests cover the rules, edge wrapping, loop detection, history limits,
seeding, button debounce and repeat, auto-reseeding, and the display text.
