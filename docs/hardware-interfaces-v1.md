# Ghost Chess v1 hardware interfaces

This document describes the electrical interface between the v1 playing board and a daughter board, plus the connections of the ATmega644PA on `ghostchess_brains_v1`.

The player's side is the side with the daughter-board slots. `A8` is the far-left square on the far rank; `A1` is the far-left square on the near rank.

## Daughter-board connector

The main-board footprints `X1` and `X2` are identical and wired in parallel. A signal driven through either slot therefore appears on the other slot. A second board must not drive `LED_DI` or `SENSOR_ON_*` while the controller board is driving them.

Use the canonical [`GhostChess_Daughter_Board` footprint](../pcb/lib/ghostchess.pretty/GhostChess_Daughter_Board.kicad_mod) when designing a compatible board. It has two 12-pin, 2.54 mm-pitch through-hole rows whose centers are 44.54 mm apart. In the footprint's unrotated component-side view, pins 1–12 run left-to-right along the upper row and pins 13–24 run left-to-right along the lower row. Pins 1 and 13 have square pads.

Directions below are relative to the main board: `in` is driven by a daughter board, `out` is driven by the playing board, and `shared` may be used by either daughter-board slot.

| Pin | Signal | Direction | Description |
|---:|---|---|---|
| 1 | GND | power | Ground |
| 2 | +5V | power | Switched/protected 5 V main-board rail; also powers the WS2812B LEDs; unused by the current daughter board |
| 3 | +3.3V | power | Main-board 3.3 V regulator output |
| 4 | `LED_DI` | in | Data input for the 64-LED WS2812B chain, through 330 Ω to `D1` |
| 5 | `SENSOR_ON_1` | in | Enable rank 8 (`U1`–`U8`) |
| 6 | `SENSOR_ON_2` | in | Enable rank 7 (`U9`–`U16`) |
| 7 | `SENSOR_ON_3` | in | Enable rank 6 (`U17`–`U24`) |
| 8 | `SENSOR_ON_4` | in | Enable rank 5 (`U25`–`U32`) |
| 9 | `SENSOR_ON_5` | in | Enable rank 4 (`U33`–`U40`) |
| 10 | `SENSOR_ON_6` | in | Enable rank 3 (`U41`–`U48`) |
| 11 | `SENSOR_ON_7` | in | Enable rank 2 (`U49`–`U56`) |
| 12 | `SENSOR_ON_8` | in | Enable rank 1 (`U57`–`U64`) |
| 13 | GND | power | Ground |
| 14 | `USER1` | shared | ATmega `PD2/RXD1`; intended extension-board receive path |
| 15 | `USER2` | shared | ATmega `PD3/TXD1`; intended extension-board transmit path |
| 16 | `USER3` | shared | ATmega `PD4/XCK1/OC1B` or GPIO |
| 17 | `SENSOR_OUT_1` | out | File A Hall-sensor outputs |
| 18 | `SENSOR_OUT_2` | out | File B Hall-sensor outputs |
| 19 | `SENSOR_OUT_3` | out | File C Hall-sensor outputs |
| 20 | `SENSOR_OUT_4` | out | File D Hall-sensor outputs |
| 21 | `SENSOR_OUT_5` | out | File E Hall-sensor outputs |
| 22 | `SENSOR_OUT_6` | out | File F Hall-sensor outputs |
| 23 | `SENSOR_OUT_7` | out | File G Hall-sensor outputs |
| 24 | `SENSOR_OUT_8` | out | File H Hall-sensor outputs |

All interface logic is 3.3 V. There is no level shifter between the ATmega and the 5 V WS2812B chain.

The schematics do not allocate a current budget to daughter boards. Check 5 V system load and 3.3 V regulator dissipation before powering a substantial extension from these pins.

## Square, sensor, and LED mapping

The Hall sensors are numbered left-to-right on every rank. The LEDs follow the serial data path and reverse direction on alternate ranks:

| Rank | `SENSOR_ON` | Sensors A→H | LEDs A→H |
|---:|---|---|---|
| 8 | 1 | `U1` … `U8` | `D1` … `D8` |
| 7 | 2 | `U9` … `U16` | `D16` … `D9` |
| 6 | 3 | `U17` … `U24` | `D17` … `D24` |
| 5 | 4 | `U25` … `U32` | `D32` … `D25` |
| 4 | 5 | `U33` … `U40` | `D33` … `D40` |
| 3 | 6 | `U41` … `U48` | `D48` … `D41` |
| 2 | 7 | `U49` … `U56` | `D49` … `D56` |
| 1 | 8 | `U57` … `U64` | `D64` … `D57` |

For file index `f = 0` for A through `7` for H and chess rank `r = 1…8`:

```text
sensor number = (8 - r) * 8 + f + 1
row enable    = SENSOR_ON_(9 - r)
column input  = SENSOR_OUT_(f + 1)

q = 8 - r
LED number = q * 8 + (q even ? f + 1 : 8 - f)
```

`D1` consumes the first LED word sent on `LED_DI`; `D64` consumes the last. `D64.DOUT` is unconnected.

## ATmega644PA connections

`U1` is an ATmega644PA-P in a 40-pin DIP package. It is powered from 3.3 V and fitted with an 11.0592 MHz crystal. Firmware clock calculations should therefore use `F_CPU = 11059200` when the external crystal is selected.

> **LED fix:** the original daughter-board PCB did not route connector pin 4 (`LED_DI`) to the MCU. On that revision, fit a patch wire from ATmega `PB0`, physical pin 1, to connector pin 4. The current schematic and v1.1 PCB layout include this connection.

| MCU pin | Port/power pin | Connected signal or circuit |
|---:|---|---|
| 1 | `PB0` | `LED_DI` via the required patch wire |
| 2 | `PB1` | Not connected |
| 3 | `PB2` | Not connected |
| 4 | `PB3` | Not connected |
| 5 | `PB4/SS` | `OLED_CS`; 10 kΩ pull-up to 3.3 V; OLED pin 16 |
| 6 | `PB5/MOSI` | `MOSI` / `OLED_SDI`; OLED pin 14 and ISP pin 1 |
| 7 | `PB6/MISO` | `MISO`; ISP pin 9 |
| 8 | `PB7/SCK` | `SCK` / `OLED_SCL`; OLED pin 12 and ISP pin 7 |
| 9 | `RESET` | Active-low reset; 10 kΩ pull-up; ISP pin 5 |
| 10 | VCC | +3.3 V |
| 11 | GND | Ground |
| 12 | `XTAL2` | 11.0592 MHz crystal, with 22 pF to ground |
| 13 | `XTAL1` | 11.0592 MHz crystal, with 22 pF to ground |
| 14 | `PD0/RXD0` | `MCU_RX`; labeled but not connected elsewhere |
| 15 | `PD1/TXD0` | `MCU_TX`; labeled but not connected elsewhere |
| 16 | `PD2/RXD1/INT0` | Connector `USER1` |
| 17 | `PD3/TXD1/INT1` | Connector `USER2` |
| 18 | `PD4/XCK1/OC1B` | Connector `USER3` |
| 19 | `PD5` | `BTN1`; switch to ground |
| 20 | `PD6` | `BTN2`; switch to ground |
| 21 | `PD7` | `BTN3`; switch to ground |
| 22 | `PC0` | `SENSOR_ON_8` (rank 1) |
| 23 | `PC1` | `SENSOR_ON_7` (rank 2) |
| 24 | `PC2/TCK` | `SENSOR_ON_6` (rank 3) |
| 25 | `PC3/TMS` | `SENSOR_ON_5` (rank 4) |
| 26 | `PC4/TDO` | `SENSOR_ON_4` (rank 5) |
| 27 | `PC5/TDI` | `SENSOR_ON_3` (rank 6) |
| 28 | `PC6` | `SENSOR_ON_2` (rank 7) |
| 29 | `PC7` | `SENSOR_ON_1` (rank 8) |
| 30 | AVCC | +3.3 V |
| 31 | GND | Ground |
| 32 | AREF | Not connected |
| 33 | `PA7/ADC7` | `SENSOR_OUT_8` (file H) |
| 34 | `PA6/ADC6` | `SENSOR_OUT_7` (file G) |
| 35 | `PA5/ADC5` | `SENSOR_OUT_6` (file F) |
| 36 | `PA4/ADC4` | `SENSOR_OUT_5` (file E) |
| 37 | `PA3/ADC3` | `SENSOR_OUT_4` (file D) |
| 38 | `PA2/ADC2` | `SENSOR_OUT_3` (file C) |
| 39 | `PA1/ADC1` | `SENSOR_OUT_2` (file B) |
| 40 | `PA0/ADC0` | `SENSOR_OUT_1` (file A) |

The three buttons have no external pull-ups. Configure `PD5`–`PD7` as inputs with the ATmega pull-ups enabled; a pressed button reads low.

The OLED is a write-only SPI peripheral in this design: `PB4` is active-low chip select, `PB5` is data, and `PB7` is clock. The display's SDO pin 13 is unconnected. `PB5`–`PB7` are also used by the ISP header.

`USER1` and `USER2` expose USART1 rather than the otherwise-unconnected USART0 nets. For a UART extension board, drive `USER1` toward the controller's `RXD1` and receive the controller's `TXD1` on `USER2`.

### ISP header

`J1` is the standard 10-pin AVR ISP arrangement:

| Pin | Signal | Pin | Signal |
|---:|---|---:|---|
| 1 | MOSI | 2 | VCC through normally-open `JP1` |
| 3 | Not connected | 4 | GND |
| 5 | RESET | 6 | GND |
| 7 | SCK | 8 | GND |
| 9 | MISO | 10 | GND |

`JP1` is open by default, so ISP pin 2 is not connected to the board's 3.3 V rail unless the jumper is bridged.

## Sensor scanning

Each `SENSOR_ON_n` drives a 2N7002 gate with a 10 kΩ pull-down. A high level turns on that rank's low-side ground switch; low turns the rank off. Keep only one rank enabled at a time.

Each file bus is shared by eight DRV5033A open-drain outputs and has no external pull-up. Configure `PA0`–`PA7` as inputs with their internal pull-ups enabled. A detected magnet pulls the selected file input low; no detected magnet reads high.

A scan can therefore:

1. Drive all `SENSOR_ON_*` signals low.
2. Drive one selected rank high.
3. Allow the sensors to power up and the bus to settle. The DRV5033 datasheet specifies a 35 µs power-on time.
4. Read `PINA`: bit 0 is file A through bit 7 file H, with `0` meaning magnet detected.
5. Drive the selected rank low before selecting the next one.

The ATmega is shipped with JTAG enabled, and `PC2`–`PC5` are the JTAG pins. JTAG must be disabled—by clearing the `JTAGEN` fuse or by the documented timed writes to `JTD`—before all eight sensor row outputs work as GPIO.

The fuses must also select the external crystal and the desired clock divider; fuse values are not encoded in the schematic. Note that 11.0592 MHz at 3.3 V is above the ATmega644PA datasheet's guaranteed 10 MHz maximum for supply voltages below 4.5 V.

## Sources

- [`pcb/v1` main-board schematic](../pcb/v1/ghostchess_v1.kicad_sch), [sensor sheet](../pcb/v1/sensors.kicad_sch), [LED sheet](../pcb/v1/leds.kicad_sch), and [PCB](../pcb/v1/ghostchess_v1.kicad_pcb)
- [`ghostchess_brains_v1` schematic](../pcb/ghostchess_brains_v1/ghostchess_brains_v1.kicad_sch) and [PCB](../pcb/ghostchess_brains_v1/ghostchess_brains_v1.kicad_pcb)
- [Microchip ATmega164A/PA/324A/PA/644A/PA/1284/P data sheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/ATmega164A_PA-324A_PA-644A_PA-1284_P_Data-Sheet-40002070B.pdf)
- [TI DRV5033 data sheet](https://www.ti.com/lit/ds/symlink/drv5033.pdf)
