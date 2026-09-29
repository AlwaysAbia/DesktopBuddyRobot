// The 76-LED round WS2812B panel: the LED buffer and each LED's position.
// Every display mode draws into led_matrix::leds; main.cpp calls FastLED.show().
#pragma once

#include <FastLED.h>

#define DATA_PIN     16   // GPIO 16
#define NUM_LEDS     76
#define BRIGHTNESS   120
#define FPS          60

namespace led_matrix {

struct Point2D {
  float x; // Horizontal in Visual Space
  float y; // Vertical in Visual Space
};

extern CRGB leds[NUM_LEDS];

// Positions in visual space, roughly the unit disk (radius ~0.95).
extern Point2D ledCoords[NUM_LEDS];

// Registers the strip with FastLED (LEDs off) and builds ledCoords.
void begin();

// LED i's position for the clock and text: ledCoords with the
// SCREEN_*_SIGN orientation from display_config.h applied. +y up, +x right.
Point2D screenPos(int i);

}  // namespace led_matrix
