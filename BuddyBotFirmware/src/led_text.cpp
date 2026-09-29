#include "led_text.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "led_matrix.h"

namespace {

// Font pixel size and scroll speed, in screen units (panel radius ~0.95).
constexpr float PIXEL        = 0.27f;  // ~2 LEDs per font pixel (the lattice is diagonal on screen)
constexpr float SCROLL_SPEED = 0.55f;  // units per second (~0.5 character/s)
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
bool fontPixel(const char* text, int len, int col, int row) {
  if (row < 0 || row >= GLYPH_ROWS || col < 0) return false;
  int index = col / CELL_COLUMNS;
  int cellCol = col % CELL_COLUMNS;
  if (index >= len || cellCol == CELL_COLUMNS - 1) return false;
  return (glyph(text[index])[row] >> (2 - cellCol)) & 1;
}

// Draws text with its left edge at screen x = left, vertically centered.
// Each LED is either fully on or off (nearest font pixel): the panel has too
// few LEDs for smoothing, which just fattens the strokes.
void draw(const char* text, int len, float left, CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    led_matrix::Point2D p = led_matrix::screenPos(i);

    // Font pixel (col, row) covers [col, col+1) x [row, row+1) in these units.
    int col = (int)floorf((p.x - left) / PIXEL);
    int row = (int)floorf(GLYPH_ROWS * 0.5f - p.y / PIXEL);
    if (fontPixel(text, len, col, row)) led_matrix::leds[i] |= color;
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
