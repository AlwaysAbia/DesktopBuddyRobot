// Scrolling text on the round LED panel: a 3x5 pixel font sampled at each
// LED's screen position (bilinear, so it moves smoothly across the sparse,
// diagonal LED grid). Lowercase prints as uppercase; unknown characters as '?'.
#pragma once

#include <FastLED.h>

namespace led_text {

// Scrolls `text` right to left through the middle of the panel, once,
// beginning at startMs (millis()). Only adds light (max-blend); it does not
// clear the panel first. Returns true once the text has fully scrolled off.
bool drawScrolling(const char* text, unsigned long startMs, CRGB color);

}  // namespace led_text
