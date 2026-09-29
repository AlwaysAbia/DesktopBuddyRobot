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

// The panel is a disc of LEDs at an even pitch: every row is centered and
// spaced the same, so the 4-LED end rows are short chords, not stretched to
// full width like ledCoords (which the eye's round shapes tolerate but a clock
// face and text do not). Same wiring order and rotation as ledCoords.
void buildPanelMap() {
  const float PITCH = 0.19f;  // 5 pitches from the center to the outer edge ~ radius 0.95
  const float rad = -135.0f * (M_PI / 180.0f);
  const float cosA = cosf(rad);
  const float sinA = sinf(rad);

  int ledIndex = 0;
  for (int r = 0; r < NUM_ROWS; r++) {
    int count = rowLengths[r];
    float y = (NUM_ROWS / 2.0f - 0.5f - r) * PITCH;

    for (int c = 0; c < count; c++) {
      int colIndex = (IS_SERPENTINE && (r % 2 != 0)) ? (count - 1 - c) : c;
      float x = (colIndex - (count - 1) / 2.0f) * PITCH;

      led_matrix::panelCoords[ledIndex].x = x * cosA - y * sinA;
      led_matrix::panelCoords[ledIndex].y = x * sinA + y * cosA;
      ledIndex++;
    }
  }
}

}  // namespace

namespace led_matrix {

CRGB leds[NUM_LEDS];
Point2D ledCoords[NUM_LEDS];
Point2D panelCoords[NUM_LEDS];

void begin() {
  // LEDs remain OFF during WiFi setup to prevent brownout
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);

  // Pre-calculate 2D matrix map
  buildCoordinateMap();
  buildPanelMap();
}

void renderTestRows() {
  int ledIndex = 0;
  for (int r = 0; r < NUM_ROWS; r++) {
    for (int c = 0; c < rowLengths[r]; c++) {
      leds[ledIndex++] = c == 0 ? CRGB(255, 255, 255) : CRGB(CHSV(r * 25, 255, 140));
    }
  }
}

Point2D screenPos(int i) {
  return { panelCoords[i].x * SCREEN_X_SIGN, panelCoords[i].y * SCREEN_Y_SIGN };
}

}  // namespace led_matrix
