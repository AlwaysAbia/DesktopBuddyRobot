// Scrolling text on the round LED panel: a 3x5 pixel font sampled at each
// LED's screen position (nearest pixel, no smoothing: the sparse, diagonal
// LED grid only stays legible with crisp on/off pixels). Lowercase prints as
// uppercase; unknown characters as '?'.
#pragma once

#include <FastLED.h>

namespace led_text {

// TEMPORARY comparison of text animations (switched by the textStyle RPC).
// SMOOTH: continuous scroll. STEPPED: scroll in whole font-pixel steps.
// LETTERS: one character at a time, no scrolling.
enum class Style : uint8_t { SMOOTH = 0, STEPPED = 1, LETTERS = 2 };
constexpr uint8_t STYLE_COUNT = 3;
void setStyle(Style style);
Style style();

// Scrolls `text` right to left through the middle of the panel, once,
// beginning at startMs (millis()). Only adds light (max-blend); it does not
// clear the panel first. Returns true once the text has fully scrolled off.
bool drawScrolling(const char* text, unsigned long startMs, CRGB color);

}  // namespace led_text
