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
  safety and pinned pieces. Promotion choice is implemented in Slice 2.
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

## Slice 2 — special moves (implemented)

En passant:

- Track eligibility after legal, in-turn double pawn moves and expire it on the
  next committed move. Lift/return cancellation preserves eligibility.
- Orange capture hints, with king safety checked after removing both pawns.
- Resolve all three-square event orders for both colors, with indefinite pauses
  between steps and an OLED completion prompt for partial landings.
- Accept uniquely inferred expired, out-of-turn, and king-exposing variants with
  advisory warnings. Extra unrelated sensor changes require restoration.
- Sanitized host tests and AVR build pass; hardware acceptance remains pending.

Castling and promotion:

- Track each original rook's castling right and both king rights. Committed
  moves and captures expire rights permanently; lift/return preserves them.
- Resolve king-first and rook-first castling, including interleaved lifts and
  landings, as one turn. Check clear paths and attacked start/transit/destination
  squares. Uniquely inferred invalid castles remain advisory.
- Wait indefinitely on partial castles. For the ambiguous rook-first landing,
  show `Castle/3 rook`: complete the castle or press BTN3 for an ordinary rook
  move. Full restoration cancels without changing turns or rights.
- Pause promotion for a button/OLED choice: BTN2 cycles queen, rook, bishop,
  knight; BTN3 confirms with a settled piece on the destination. Allow physical
  pawn replacement before confirmation and full restoration to cancel.
- Debounce BTN2/BTN3 without repeat; retain BTN1's two-second reset hold.
- Sanitized host tests cover rights, attacked squares, both colors/sides and all
  six physical castling event interleavings, promotion choices and capture
  orders, cancellation, advisory-invalid variants, and button handling.

Placement acknowledgement (brought forward from Slice 3):

- Every debounced placement during play pulses green for 350 ms at half hint
  brightness, including captures, returns, and partial special moves. This
  acknowledges sensor detection, independently of move legality or commitment.
- Scanning continues during the pulse. Existing hints, check indications, and
  advisory warnings reappear after it ends.
- Host tests and AVR build pass; hardware acceptance remains pending.

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
7. Play en passant for both colors, removing the victim before and after landing
   the attacker. Pause between steps; verify orange hints, the completion prompt,
   and a single turn change. Restore a partial gesture to cancel. Repeat after
   eligibility has expired and verify advisory red feedback.
8. Verify a subtle green pulse on each detected landing (ordinary move, capture,
   lift/return,
   and partial special move). Confirm no pulse occurs without a detected
   placement, hints resume afterward, invalid-move warnings remain visible,
   and immediate subsequent moves are still sensed during the pulse.
9. Castle both sides with both colors, king-first and rook-first, pausing between
   lifts and landings. Verify one turn change, blue legal king destinations, and
   `Finish castling` / `Castle/3 rook` prompts. Confirm a rook-only move with BTN3.
   Restore partial gestures to cancel. Repeat after king/rook move-and-return
   and through attacked squares; verify red advisory warnings and no legal hint.
10. Promote both colors, with and without capture, to each of the four choices.
    Use BTN2 to cycle, swap the physical pawn, then BTN3 to confirm. Check that
    an empty destination or unrelated change prevents confirmation, cancellation
    preserves the turn, and holding a choice button does not repeat.
11. Hold BTN1 during a pending castle or promotion; verify setup and all initial
    rights are restored. Check button bounce and simultaneous choice/confirm
    presses do not accidentally commit a move.

Current limits and observable sensor ambiguities are detailed in the
[firmware README](../firmware/README.md). No firmware upload has been performed.
