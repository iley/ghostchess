# First functional firmware

The board is an assistant: track observed moves, offer legal destinations, and
warn about invalid moves without blocking play. Sensors report only occupancy;
piece identity is inferred from a standard setup and subsequent gestures.

## Slice 1 — implemented

- Standard-position setup gate; short green placement feedback; red for extra
  pieces; automatic start after a stable complete setup.
- Portable chess and gesture modules, separated from the AVR sensor/LED driver.
- Ordinary moves and captures in either removal order, lift/return cancellation,
  settling to ignore brief landings, and conservative handling of ambiguity.
- Blue legal destinations, orange captures and checked kings, including king
  safety and pinned pieces. Automatic queen promotion.
- Advisory invalid-move acceptance with a 2.2-second red blink; opposite color
  moves next, even following an out-of-turn move.
- OLED status, dim board lighting, nonblocking feedback, and BTN1 reset to setup.
- Host tests with sanitizers and opening move-count checks; successful AVR build.

`chess.c` owns piece movement and king safety. `assistant.c` consumes complete
debounced occupancy snapshots and owns setup, gesture inference, hints, and
feedback timers. `main.c` retains the hardware scan/serpentine LED mapping and
uses a polled hardware timer unaffected by WS2812 interrupt suppression.

The tracked position stays unchanged during a lift. A move needs one missing
source plus one newly occupied destination; a capture instead needs a previously
observed removal and replacement at its destination. Full occupancy restoration
cancels the gesture. Legality never gates a uniquely inferred move.

## Slice 2 — special moves

- Track castling rights and resolve king/rook gestures as one turn, in either
  handling order; validate attacked transit squares.
- Track en passant eligibility and resolve its three-square gesture in practical
  removal/landing orders.
- Add button/OLED promotion choice and test replacement of the physical pawn.
- Extend rules and gesture tests to cover special moves, cancellation, and
  permissive invalid variants without prematurely committing partial gestures.

## Slice 3 — recovery and board validation

- Recovery/undo interaction for ambiguous captures, accidental extra pieces,
  piece corrections, and arbitrary-position setup. Occupancy alone cannot
  distinguish all these intentions; use the available buttons/OLED.
- Consider explicit start confirmation and game-over information after board
  testing. Retain advisory behavior rather than locking play.
- Tune debounce, settling, LED brightness, and colors on the assembled board.

## Hardware acceptance checks (still pending)

1. Start empty, populate squares slowly, verify each green pulse and orientation.
   Confirm a missing piece or extra center-square piece prevents automatic start.
2. Lift/return a pawn and knight; verify hints and unchanged turn. Move normally.
3. Capture attacker-first and victim-first, with a pause while both are lifted.
   Verify the destination now tracks the attacker. Cancel before landing too.
4. Leave an illegal move in place, observe red blinking, and immediately continue
   with the other color. Verify no sensor events are lost during feedback.
5. Create check and a pinned piece; verify orange king and restricted hints.
6. Hold BTN1 to reset; verify setup restarts without repeated resets while held.

Current limits and observable sensor ambiguities are detailed in the
[firmware README](../firmware/README.md). No firmware upload has been performed.
