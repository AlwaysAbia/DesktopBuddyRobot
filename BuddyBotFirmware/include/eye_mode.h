// EYE_ANIMATION mode: the gazing, blinking eye.
#pragma once

#include <stdint.h>

namespace eye {

// Color themes, see THEMES[] in eye_mode.cpp. Amber is the boot default.
enum Theme : uint8_t { THEME_CYAN = 0, THEME_AMBER = 1, THEME_EMERALD = 2, THEME_MAGENTA = 3, THEME_COUNT = 4 };

// Takes effect on the next frame. Out-of-range values are ignored.
// Nothing calls this yet; BLE eye color (next session) will.
void setTheme(uint8_t theme);
uint8_t theme();

// Advances gaze + blink and draws one frame into led_matrix::leds.
void render();

}  // namespace eye
