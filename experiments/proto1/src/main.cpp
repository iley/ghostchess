#include <Arduino.h>
#include <FastLED.h>

constexpr uint8_t kLedDataPin = 3;
constexpr size_t kNumLeds = 4;
constexpr uint8_t kBrightness = 128;

constexpr size_t kNumRows = 2;
constexpr size_t kNumCols = 2;

const uint8_t sensorPowerPins[kNumRows] = {2, 5};
const uint8_t sensorOutPins[kNumCols] = {4, 7};

constexpr int kSensorPowerDelay = 2;

CRGB leds[kNumLeds];

void setup() {
  FastLED.addLeds<WS2812B, kLedDataPin, GRB>(leds, kNumLeds)
      .setCorrection(TypicalLEDStrip);

  for (int i = 0; i < kNumRows; i++) {
    pinMode(sensorPowerPins[i], OUTPUT);
  }

  for (int i = 0; i < kNumCols; i++) {
    pinMode(sensorOutPins[i], INPUT_PULLUP);
  }

  digitalWrite(sensorPowerPins[0], HIGH);
}

int ledNumber(int row, int col) {
  if (row == 0 && col == 0) {
    return 0;
  } else if (row == 0 && col == 1) {
    return 1;
  } else if (row == 1 && col == 1) {
    return 3;
  } else {
    return 2;
  }
}

void setLed(int row, int col, bool on) {
  int n = ledNumber(row, col);
  if (on) {
    leds[n] = CRGB::White;
  } else {
    leds[n] = CRGB::Black;
  }
}

void loop() {
  fill_solid(leds, kNumLeds, CRGB::Black);

  for (int row = 0; row < kNumRows; row++) {
    digitalWrite(sensorPowerPins[row], HIGH);
    delay(kSensorPowerDelay);

    for (int col = 0; col < kNumCols; col++) {
      int sensorValue = digitalRead(sensorOutPins[col]);
      bool magnetPresent = sensorValue == LOW;
      setLed(row, col, magnetPresent);
    }

    digitalWrite(sensorPowerPins[row], LOW);
    delay(kSensorPowerDelay);
  }

  FastLED.show();
}
