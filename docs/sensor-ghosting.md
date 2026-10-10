# Sensor ghosting on the v1 board

Two pieces on different ranks and files reliably produce a phantom detection at
one of the other corners of their rectangle: for example, A1 and C3 can cause A3
to register as occupied.

The suspected cause is electrical coupling through inactive ranks. The v1 board
switches each rank's sensor grounds with a 2N7002, while keeping sensor VCC at
3.3 V and connecting all eight outputs on each file together. A low file bus may
provide an unintended return path through an inactive sensor into its rank's
floating ground, allowing another sensor on that rank to pull a second file low.
This mechanism has not yet been confirmed by measurement. Firmware delays and
debounce cannot reliably distinguish a sustained electrical phantom from a real
piece.

## Proposed hardware fix — not yet validated

Insert **one series Schottky diode per sensor output (64 total)**, before the
output joins its shared file bus:

```text
Shared file bus -- anode [diode] cathode -- sensor OUT
                               banded end
```

The cathode/band faces the sensor. Interrupt the existing OUT-to-bus connection
and bridge it with the diode. This should permit normal sensing current into OUT
while blocking reverse current from the sensor onto a low file bus.

Suggested parts are **Nexperia BAT85** (axial DO-34, convenient for wire repairs)
or **Nexperia BAT54GW** (two-terminal SOD-123). Both specify a maximum forward
drop of 0.24 V at 0.1 mA and 0.32 V at 1 mA, at 25°C. The low drop leaves margin
below the ATmega's 0.99 V guaranteed-low limit at a 3.3 V supply; verify the total
diode, sensor, and ground-switch drop on the board.

Before modifying all 64 outputs, isolate all eight outputs on one rank and repeat
the two-piece tests against other ranks. Check raw sensor readings, genuine
detections, and removal/release behavior. Measure bus low/high levels while that
rank is selected, including with several pieces present. Only extend the repair
after confirming that it removes ghosting without losing real detections.

References: [v1 interfaces](hardware-interfaces-v1.md),
[DRV5033 datasheet](https://www.ti.com/lit/ds/symlink/drv5033.pdf),
[BAT85 datasheet](https://assets.nexperia.com/documents/data-sheet/BAT85.pdf),
[BAT54GW datasheet](https://assets.nexperia.com/documents/data-sheet/BAT54GW.pdf),
[ATmega datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU08/ProductDocuments/DataSheets/ATmega164A_PA-324A_PA-644A_PA-1284_P_Data-Sheet-40002070B.pdf).
