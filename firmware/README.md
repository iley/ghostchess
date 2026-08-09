# Ghost Chess firmware

Initial bare-metal AVR firmware for the `ghostchess_brains_v1` ATmega644PA.
It displays an 8x8 chessboard by lighting the light squares white at half RGB
level, scans all 64 Hall sensors continuously, and flashes a newly occupied
square green at full RGB level for 500 ms. A square is re-armed after its
sensor reads unoccupied.

## Build

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html),
then run:

```sh
cd firmware
pio run
```

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
