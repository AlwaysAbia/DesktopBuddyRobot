#include "display_modes.h"

#include <Arduino.h>
#include <WiFi.h>

#include "clock_mode.h"
#include "eye_mode.h"
#include "led_matrix.h"
#include "messages.h"

namespace {

const CRGB STATUS_WIFI_UP   = CRGB(0, 120, 0);
const CRGB STATUS_WIFI_DOWN = CRGB(60, 0, 0);

modes::Mode currentMode = modes::Mode::EYE_ANIMATION;
int statusLed = 0;

// The outermost LED toward the top-right: the round panel's closest thing to a corner.
int findStatusLed() {
  int best = 0;
  for (int i = 1; i < NUM_LEDS; i++) {
    led_matrix::Point2D p = led_matrix::screenPos(i);
    led_matrix::Point2D b = led_matrix::screenPos(best);
    if (p.x + p.y > b.x + b.y) best = i;
  }
  return best;
}

}  // namespace

namespace modes {

void begin() {
  statusLed = findStatusLed();
}

void set(Mode mode) {
  if ((uint8_t)mode >= MODE_COUNT) return;
  currentMode = mode;
  if (mode == Mode::CURRENT_TIME) clock_mode::onEnter();
  if (mode == Mode::MESSAGE_HISTORY) messages::onEnter();
  Serial.printf("[Mode] %s\r\n", name(mode));
}

void next() {
  set((Mode)(((uint8_t)currentMode + 1) % MODE_COUNT));
}

Mode current() {
  return currentMode;
}

const char* name(Mode mode) {
  switch (mode) {
    case Mode::EYE_ANIMATION:   return "EYE_ANIMATION";
    case Mode::CURRENT_TIME:    return "CURRENT_TIME";
    case Mode::MESSAGE_HISTORY: return "MESSAGE_HISTORY";
    default:                    return "UNKNOWN";
  }
}

void render() {
  switch (currentMode) {
    case Mode::EYE_ANIMATION:
      eye::render();
      return;  // the eye never shows the status LED
    case Mode::CURRENT_TIME:
      clock_mode::render();
      break;
    case Mode::MESSAGE_HISTORY:
      messages::render();
      break;
  }
  led_matrix::leds[statusLed] = WiFi.status() == WL_CONNECTED ? STATUS_WIFI_UP : STATUS_WIFI_DOWN;
}

}  // namespace modes
