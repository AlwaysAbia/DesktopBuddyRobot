// Screen/mode manager: which mode owns the LED panel, and the WiFi status LED
// shown in CURRENT_TIME and MESSAGE_HISTORY (never in EYE_ANIMATION).
#pragma once

#include <stdint.h>

namespace modes {

// Numbering matches the BLE "Mode Select" values in interface-contract.md.
enum class Mode : uint8_t { EYE_ANIMATION = 0, CURRENT_TIME = 1, MESSAGE_HISTORY = 2 };
constexpr uint8_t MODE_COUNT = 3;

// Picks the status LED. Call after led_matrix::begin(). Boots in EYE_ANIMATION.
void begin();

void set(Mode mode);
void next();
Mode current();
const char* name(Mode mode);

// Draws the current mode's frame (plus the status LED) into led_matrix::leds.
void render();

}  // namespace modes
