/*
 * GhostChess brains v1 — 1602 OLED smoke test.
 *
 * Drives a 16x2 character OLED (Winstar WS0010 / OLED-0010 controller) over
 * its serial/SPI interface and shows a title plus a live uptime counter.
 *
 * The WS0010 serial protocol is bit-banged by the library, and it carries the
 * register-select (command vs data) bit inside the bitstream — so unlike a
 * graphical SPI display there is NO separate DC/RS pin. We only wire clock,
 * data-in and chip-select.
 *
 * Wiring (Arduino Nano <-> OLED, see notes at bottom of file):
 *   D13 -> SCL  (clock,  SPI_SCK_PIN  in the library)
 *   D11 -> SDI  (data in, SPI_MOSI_PIN in the library)
 *   D10 -> CS   (chip select / enable, passed to the constructor below)
 *   SDO  : leave unconnected (write-only; library never reads it)
 */

#include <Arduino.h>
#include <Silvervest_OLED_0010_SPI.h>

// Chip-select pin. The clock (D13) and data (D11) pins are fixed in the
// library header; only CS is our choice. D10 is the ATmega328P's hardware SS
// pin, the conventional spot for it.
static const uint8_t OLED_CS_PIN = 10;

static Silvervest_OLED_0010_SPI oled(OLED_CS_PIN);

void setup() {
  oled.begin(16, 2);
  oled.print("GhostChess v1");
}

// Print the seconds since boot on the second row, refreshed once a second.
void loop() {
  oled.setCursor(0, 1);
  oled.print("up: ");
  oled.print(millis() / 1000);
  oled.print("s   "); // trailing spaces clear digits left over as the count shrinks in width
  delay(1000);
}
