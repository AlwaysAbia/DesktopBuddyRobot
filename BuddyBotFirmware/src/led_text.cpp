#include "led_text.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "led_matrix.h"

namespace {

// Font pixel size and scroll speed, in screen units (panel radius ~0.95).
constexpr float PIXEL        = 0.20f;
constexpr float SCROLL_SPEED = 0.80f;  // units per second (~1 character/s)
constexpr float EDGE         = 1.0f;   // text enters at +EDGE, leaves at -EDGE

constexpr int GLYPH_ROWS    = 5;
constexpr int CELL_COLUMNS  = 4;  // 3 glyph columns + 1 blank

// ASCII ' ' (32) to 'Z' (90). One byte per row, top row first, bit 2 = left column.
const uint8_t FONT[][GLYPH_ROWS] = {
  {0b000, 0b000, 0b000, 0b000, 0b000},  // ' '
  {0b010, 0b010, 0b010, 0b000, 0b010},  // '!'
  {0b101, 0b101, 0b000, 0b000, 0b000},  // '"'
  {0b101, 0b111, 0b101, 0b111, 0b101},  // '#'
  {0b011, 0b110, 0b010, 0b011, 0b110},  // '$'
  {0b101, 0b001, 0b010, 0b100, 0b101},  // '%'
  {0b010, 0b101, 0b010, 0b101, 0b011},  // '&'
  {0b010, 0b010, 0b000, 0b000, 0b000},  // '''
  {0b001, 0b010, 0b010, 0b010, 0b001},  // '('
  {0b100, 0b010, 0b010, 0b010, 0b100},  // ')'
  {0b000, 0b101, 0b010, 0b101, 0b000},  // '*'
  {0b000, 0b010, 0b111, 0b010, 0b000},  // '+'
  {0b000, 0b000, 0b000, 0b010, 0b100},  // ','
  {0b000, 0b000, 0b111, 0b000, 0b000},  // '-'
  {0b000, 0b000, 0b000, 0b000, 0b010},  // '.'
  {0b001, 0b001, 0b010, 0b100, 0b100},  // '/'
  {0b111, 0b101, 0b101, 0b101, 0b111},  // '0'
  {0b010, 0b110, 0b010, 0b010, 0b111},  // '1'
  {0b111, 0b001, 0b111, 0b100, 0b111},  // '2'
  {0b111, 0b001, 0b111, 0b001, 0b111},  // '3'
  {0b101, 0b101, 0b111, 0b001, 0b001},  // '4'
  {0b111, 0b100, 0b111, 0b001, 0b111},  // '5'
  {0b111, 0b100, 0b111, 0b101, 0b111},  // '6'
  {0b111, 0b001, 0b001, 0b001, 0b001},  // '7'
  {0b111, 0b101, 0b111, 0b101, 0b111},  // '8'
  {0b111, 0b101, 0b111, 0b001, 0b111},  // '9'
  {0b000, 0b010, 0b000, 0b010, 0b000},  // ':'
  {0b000, 0b010, 0b000, 0b010, 0b100},  // ';'
  {0b001, 0b010, 0b100, 0b010, 0b001},  // '<'
  {0b000, 0b111, 0b000, 0b111, 0b000},  // '='
  {0b100, 0b010, 0b001, 0b010, 0b100},  // '>'
  {0b111, 0b001, 0b010, 0b000, 0b010},  // '?'
  {0b010, 0b101, 0b111, 0b100, 0b011},  // '@'
  {0b010, 0b101, 0b111, 0b101, 0b101},  // 'A'
  {0b110, 0b101, 0b110, 0b101, 0b110},  // 'B'
  {0b011, 0b100, 0b100, 0b100, 0b011},  // 'C'
  {0b110, 0b101, 0b101, 0b101, 0b110},  // 'D'
  {0b111, 0b100, 0b110, 0b100, 0b111},  // 'E'
  {0b111, 0b100, 0b110, 0b100, 0b100},  // 'F'
  {0b011, 0b100, 0b101, 0b101, 0b011},  // 'G'
  {0b101, 0b101, 0b111, 0b101, 0b101},  // 'H'
  {0b111, 0b010, 0b010, 0b010, 0b111},  // 'I'
  {0b001, 0b001, 0b001, 0b101, 0b010},  // 'J'
  {0b101, 0b101, 0b110, 0b101, 0b101},  // 'K'
  {0b100, 0b100, 0b100, 0b100, 0b111},  // 'L'
  {0b101, 0b111, 0b111, 0b101, 0b101},  // 'M'
  {0b110, 0b101, 0b101, 0b101, 0b101},  // 'N'
  {0b010, 0b101, 0b101, 0b101, 0b010},  // 'O'
  {0b110, 0b101, 0b110, 0b100, 0b100},  // 'P'
  {0b010, 0b101, 0b101, 0b110, 0b011},  // 'Q'
  {0b110, 0b101, 0b110, 0b101, 0b101},  // 'R'
  {0b011, 0b100, 0b010, 0b001, 0b110},  // 'S'
  {0b111, 0b010, 0b010, 0b010, 0b010},  // 'T'
  {0b101, 0b101, 0b101, 0b101, 0b111},  // 'U'
  {0b101, 0b101, 0b101, 0b101, 0b010},  // 'V'
  {0b101, 0b101, 0b111, 0b111, 0b101},  // 'W'
  {0b101, 0b101, 0b010, 0b101, 0b101},  // 'X'
  {0b101, 0b101, 0b010, 0b010, 0b010},  // 'Y'
  {0b111, 0b001, 0b010, 0b100, 0b111},  // 'Z'
};
static_assert(sizeof(FONT) / sizeof(FONT[0]) == 'Z' - ' ' + 1, "FONT must cover ' ' to 'Z'");

const uint8_t* glyph(char c) {
  if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  if (c < ' ' || c > 'Z') c = '?';
  return FONT[c - ' '];
}

// 1 if font pixel (col, row) of the whole string is lit.
float fontPixel(const char* text, int len, int col, int row) {
  if (row < 0 || row >= GLYPH_ROWS || col < 0) return 0.0f;
  int index = col / CELL_COLUMNS;
  int cellCol = col % CELL_COLUMNS;
  if (index >= len || cellCol == CELL_COLUMNS - 1) return 0.0f;
  return (glyph(text[index])[row] >> (2 - cellCol)) & 1 ? 1.0f : 0.0f;
}

// Draws text with its left edge at screen x = left, vertically centered.
void draw(const char* text, int len, float left, CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    led_matrix::Point2D p = led_matrix::screenPos(i);

    // Position in font pixels; pixel (c, r) has its center at (c + 0.5, r + 0.5).
    float u = (p.x - left) / PIXEL - 0.5f;
    float v = (GLYPH_ROWS * 0.5f) - p.y / PIXEL - 0.5f;
    if (u < -1.0f || u > len * CELL_COLUMNS || v < -1.0f || v > GLYPH_ROWS) continue;

    int c0 = (int)floorf(u);
    int r0 = (int)floorf(v);
    float fu = u - c0;
    float fv = v - r0;
    float value = fontPixel(text, len, c0,     r0)     * (1 - fu) * (1 - fv)
                + fontPixel(text, len, c0 + 1, r0)     * fu       * (1 - fv)
                + fontPixel(text, len, c0,     r0 + 1) * (1 - fu) * fv
                + fontPixel(text, len, c0 + 1, r0 + 1) * fu       * fv;
    if (value < 0.05f) continue;

    CRGB lit = color;
    lit.nscale8((uint8_t)(value * 255.0f));
    led_matrix::leds[i] |= lit;
  }
}

}  // namespace

namespace led_text {

bool drawScrolling(const char* text, unsigned long startMs, CRGB color) {
  int len = strlen(text);
  float width = (len * CELL_COLUMNS - 1) * PIXEL;
  float left = EDGE - SCROLL_SPEED * (millis() - startMs) / 1000.0f;
  if (left + width < -EDGE) return true;
  draw(text, len, left, color);
  return false;
}

}  // namespace led_text
