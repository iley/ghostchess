# Ghost Chess firmware

Bare-metal AVR firmware for the `ghostchess_brains_v1` ATmega644PA.
The firmware tracks ordinary moves, en passant, castling, and promotion, with
advisory LED feedback. See the [implementation plan](../docs/firmware-plan.md)
for remaining recovery work and hardware acceptance checks.

## Playing

1. Arrange the standard starting position: white on ranks 1–2, black on 7–8,
   with queens on D1/D8. The sensors detect occupancy, not piece identity, so
   the player must put the correct pieces on the correct squares.
2. Each occupied starting square briefly lights green. Extra pieces on ranks
   3–6 light red. The game starts with White once all 32 starting squares are
   occupied, all other squares are empty, and the board is stable for 600 ms.
3. Lift a piece: legal empty destinations light blue; legal captures light
   orange. Hints respect blockers, pawn direction, turn, and king safety.
   A checked king also lights orange.
4. Put it back to cancel, without changing turns. For a capture, remove the
   opponent's piece and move yours onto its square; either removal order works.
   Give each removal time to register (roughly 120 ms with the current scanner).
   En passant destinations also light orange. Move the pawn and remove the
   captured pawn in either order; the board waits for all three squares to
   settle before changing turns. `Finish en passant` means a removal is still
   pending. Restore the tracked position to cancel.
5. Castle by moving the king and rook, in either order. The board waits for both
   pieces, even with long pauses, and changes turns once. If the rook lands first,
   `Castle/3 rook` means finish castling or press BTN3 to commit a rook-only move.
   Restore both pieces to cancel. Castling through check or after losing rights
   is accepted with an advisory warning when the gesture is identifiable.
6. On promotion, BTN2 cycles queen, rook, bishop, knight; BTN3 confirms. Replace
   the pawn with the selected piece before confirming. The board waits while the
   destination is empty and requires a stable placement. Restore the original
   position to cancel. Sensors cannot verify the replacement's identity.
7. Every detected placement briefly pulses green (350 ms), including captures
   and returning a lifted piece. This confirms detection, not legality or a turn
   change. Ordinary landings stable for 180 ms after sensor debounce commit the
   move; special moves wait for completion or confirmation. Invalid moves blink
   red for 2.2 seconds, visible after any green pulse ends. Play can continue
   during feedback. After any accepted move, the opposite color moves next.
8. Hold BTN1 for two seconds to reset to setup, or power-cycle. Games are not
   saved across resets.

The background board is dim white; hints use restrained brightness. Setup
and placement pulses, move settling, and warning animation do not block sensor
scanning. BTN2/BTN3 act once per debounced press; holding them does not repeat.
The OLED shows setup, the next side, or a request to restore ambiguous handling.

### Current limits

- A rook moving from its home corner to its castling destination with the king
  still home is ambiguous, even after rights have expired. BTN3 confirms an
  ordinary rook move; no timeout guesses the intent. Castle recognition requires
  the matching king/rook on their home squares and empty landing squares in the
  tracked position. Other changes may require restoration.
- En passant eligibility lasts for the reply to a legal, in-turn double pawn
  move. Expired, out-of-turn, and king-exposing en passant gestures are still
  tracked, with an invalid-move warning. An en passant-shaped diagonal landing
  beside an opposing pawn waits for its removal even when the move is illegal.
  Occupancy alone cannot distinguish this from an intended illegal ordinary move;
  restore the pieces to cancel. Removing the victim and placing a piece on the
  destination before lifting the attacker also waits for completion.
- Promotion must be confirmed before starting the next move; unrelated board
  changes show `Restore pieces` and prevent confirmation.
- Move one piece at a time (plus its capture victim or castling partner). Multiple
  unrelated lifts are not reliably identifiable. Restore the last tracked position when the
  OLED asks `Restore pieces`, or reset and set up again.
- Hall sensors cannot detect piece swaps, confirm the correct starting identities,
  or recognize a capture when the victim's empty-square interval is missed.
  Likewise, replacing a victim while the attacker remains lifted looks exactly
  like completing a capture. To cancel that gesture, restore the attacker first,
  then the victim, or restore both within the settling window.
- An accepted illegal position remains playable, with subsequent hints based on
  the tracked pieces. There is no undo, arbitrary-position editor, or end-game
  adjudication yet.

## Build

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html),
then run:

```sh
cd firmware
pio run
```

### Apple Silicon

The pinned PlatformIO AVR package contains Intel binaries. The
[`native_avr.py`](scripts/native_avr.py) build hook automatically selects native
Homebrew tools on ARM Macs, for both compilation and upload. Other hosts keep
PlatformIO's packaged tools. This uses PlatformIO's supported
[post-script environments](https://docs.platformio.org/en/stable/scripting/construction_environments.html).

Install the dependencies if they are not present:

```sh
brew install osx-cross/avr/avr-gcc@8 osx-cross/avr/avr-binutils avrdude isl libmpc
```

Homebrew may require you to trust the individual `osx-cross/avr` formulae before
installing them. Use the formula-specific instructions Homebrew prints.
The build hook discovers the Homebrew prefix and does not modify PlatformIO's
package cache. Its package listing still shows the bundled compiler; the
`Using native Homebrew AVR toolchain (Apple Silicon)` line confirms the override.

The project deliberately has no Arduino framework dependency; it is compiled
as C against `avr-libc`. The default upload protocol is USBasp over the board's
10-pin ISP header:

```sh
pio run --target upload
```

Change `upload_protocol` in `platformio.ini` if a different ISP programmer is
used. ISP pin 2 only powers the target when `JP1` is bridged; otherwise power
the board separately and keep programmer and board grounds connected.

## Hardware assumptions

- CPU: ATmega644PA with the 11.0592 MHz external crystal, clock prescaler 1.
- LED data: `PB0`, including the patch wire required on the original daughter
  board revision.
- Rank enables: `PC7..PC0` for chess ranks 8..1. Firmware disables JTAG at
  runtime so all eight pins work as GPIO.
- Sensor inputs: `PA0..PA7` for files A..H, using internal pull-ups. A low
  input means a magnet is present.
- Buttons: `PD5`/BTN1 reset, `PD6`/BTN2 promotion choice, `PD7`/BTN3 confirm;
  active-low with internal pull-ups and 50 ms debounce.
- LED color order: WS2812 GRB, with the documented serpentine rank mapping.

The firmware does not program fuses. Before running it, configure the fuses to
select the external crystal. It removes `CKDIV8` at runtime, and disables JTAG
at runtime, so neither fuse has to be changed for normal operation.

Program the recommended fuses with a USBasp using:

```sh
./program-fuses.sh
```

This writes high fuse `0xD9`, extended fuse `0xFD`, and low fuse `0xF7`. The
programmer, ISP bit clock, AVRDUDE executable, and optional programmer port can
be overridden with `AVR_PROGRAMMER`, `AVR_BITCLOCK`, `AVRDUDE_BIN`, and
`AVR_PROGRAMMER_PORT`, respectively.

The board design drives a 5 V WS2812B data input directly from 3.3 V logic.
That is part of the v1 hardware interface and may have limited logic-high
margin. Also note that 11.0592 MHz at 3.3 V is outside the ATmega644PA's
datasheet-guaranteed operating region documented for this board.

## Host verification

The chess and gesture logic have no AVR dependencies. Run their tests with a
C compiler supporting AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
sh firmware/tests/run.sh  # from the repository root
```

`CC` can select a different compiler. If an installed macOS beta SDK and linker
are incompatible, select a compatible installed SDK with `SDKROOT`. On the
current development machine, the verified command is:

```sh
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk sh firmware/tests/run.sh
```

The tests exercise setup, hints, capture orders, cancellation, advisory invalid
moves, ambiguous handling, promotion, check/pins, timing rollover, en passant for
both colors in all six sensor-event orders (including long pauses), eligibility
expiry, cancellation, and king safety after removing both pawns. Castling tests
cover both sides/colors, all six physical lift/landing interleavings with pauses,
rights loss, attacked squares, cancellation, and rook-only confirmation.
Promotion tests cover all four choices, both colors, both capture orders,
physical replacement, cancellation, and invalid moves. Button bounce, held-button
behavior, reset across timer rollover, and placement pulses are also exercised.
Opening move counts of 20 / 400 / 8,902 through three plies remain checked. Sensor
electrical behavior, LED timing/color, and human gesture timing still need
verification on the board.
