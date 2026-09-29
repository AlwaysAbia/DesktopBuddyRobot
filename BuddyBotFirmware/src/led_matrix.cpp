#include "led_matrix.h"

#include <math.h>

#include "display_config.h"

namespace {

const int NUM_ROWS = 10;
const int rowLengths[NUM_ROWS] = {4, 6, 8, 10, 10, 10, 10, 8, 6, 4};
const bool IS_SERPENTINE = true;

void buildCoordinateMap() {
  int ledIndex = 0;

  // Rotated angle (-135.0f) for matrix alignment
  const float rad = -135.0f * (M_PI / 180.0f);
  const float cosA = cosf(rad);
  const float sinA = sinf(rad);

  for (int r = 0; r < NUM_ROWS; r++) {
    int count = rowLengths[r];
    float rawY = 1.0f - (2.0f * (r + 0.5f) / (float)NUM_ROWS);

    for (int c = 0; c < count; c++) {
      // Handle serpentine / zigzag row wiring direction
      int colIndex = (IS_SERPENTINE && (r % 2 != 0)) ? (count - 1 - c) : c;

      float rawX = -1.0f + (2.0f * (colIndex + 0.5f) / (float)count);

      rawX *= 0.95f;
      rawY *= 0.95f;

      // Rotational matrix transform
      float rotatedX = rawX * cosA - rawY * sinA;
      float rotatedY = rawX * sinA + rawY * cosA;

      led_matrix::ledCoords[ledIndex].x = rotatedX;
      led_matrix::ledCoords[ledIndex].y = rotatedY;

      ledIndex++;
    }
  }
}

}  // namespace

namespace led_matrix {

CRGB leds[NUM_LEDS];
Point2D ledCoords[NUM_LEDS];

void begin() {
  // LEDs remain OFF during WiFi setup to prevent brownout
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);

  // Pre-calculate 2D matrix map
  buildCoordinateMap();
}

Point2D screenPos(int i) {
  return { ledCoords[i].x * SCREEN_X_SIGN, ledCoords[i].y * SCREEN_Y_SIGN };
}

}  // namespace led_matrix
