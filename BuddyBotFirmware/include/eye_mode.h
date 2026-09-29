// EYE_ANIMATION mode: the gazing, blinking eye.
#pragma once

#include <stdint.h>

namespace eye {

// Color themes, see THEMES[] in eye_mode.cpp. Amber is the boot default.
enum Theme : uint8_t { THEME_CYAN = 0, THEME_AMBER = 1, THEME_EMERALD = 2, THEME_MAGENTA = 3, THEME_COUNT = 4 };

// Loads the stored theme from NVS (display/eye_theme). Call once in setup().
// Until the first setTheme() there is nothing stored and Amber applies.
void begin();

// Takes effect on the next frame and is saved to NVS if it changed.
// Out-of-range values are ignored. Call from the main loop task.
void setTheme(uint8_t theme);
uint8_t theme();

// Advances gaze + blink and draws one frame into led_matrix::leds.
void render();

}  // namespace eye
