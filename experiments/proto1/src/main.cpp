#include <Arduino.h>
#include <FastLED.h>

constexpr uint8_t kLedDataPin = 3;
constexpr size_t kNumLeds = 4;
constexpr uint8_t kBrightness = 128;

CRGB leds[kNumLeds];

void setup() {
  FastLED.addLeds<WS2812B, kLedDataPin, GRB>(leds, kNumLeds)
      .setCorrection(TypicalLEDStrip);
}

void lightLed(size_t n) {
  fill_solid(leds, kNumLeds, CRGB::Black);
  leds[n] = CRGB::White;
  FastLED.show();
}

void spin() {
  lightLed(0);
  delay(500);

  lightLed(2);
  delay(500);

  lightLed(3);
  delay(500);

  lightLed(1);
  delay(500);
}

void rainbow() {
  static uint8_t hue = 0;
  for (size_t i = 0; i < kNumLeds; ++i) {
    leds[i] = CHSV(hue + i * 8, 255, 255);
  }
  FastLED.show();
  ++hue;
  delay(20);
}

void staticLight() {
  fill_solid(leds, kNumLeds, CRGB::White);
  FastLED.show();
  delay(2000);
}

void loop() {
  spin();

  for (int i = 0; i < 200; i++) {
    rainbow();
  }

  staticLight();
}
