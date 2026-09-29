#include "led_text.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "display_config.h"
#include "led_matrix.h"

namespace {

// Font pixel size and scroll speed, in screen units (panel radius ~0.95).
constexpr float PIXEL        = 0.19f;  // one font pixel = one LED pitch (text is tilted onto the LED grid)
constexpr float SCROLL_SPEED = 0.50f;          // units per second for the smooth style (~0.65 character/s)
constexpr float EDGE         = 1.0f;   // text enters at +EDGE, leaves at -EDGE

const float TEXT_COS = cosf(TEXT_ROTATION_DEG * (float)M_PI / 180.0f);
const float TEXT_SIN = sinf(TEXT_ROTATION_DEG * (float)M_PI / 180.0f);

constexpr int GLYPH_ROWS    = 7;
constexpr int GLYPH_COLUMNS = 5;
constexpr int CELL_COLUMNS  = 6;  // 5 glyph columns + 1 blank

// ASCII ' ' (32) to 'Z' (90). One byte per row, top row first, bit 4 = left column.
const uint8_t FONT[][GLYPH_ROWS] = {
  {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000},  //  
  {0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b00100},  // !
  {0b01010, 0b01010, 0b01010, 0b00000, 0b00000, 0b00000, 0b00000},  // "
  {0b01010, 0b01010, 0b11111, 0b01010, 0b11111, 0b01010, 0b01010},  // #
  {0b00100, 0b01111, 0b10100, 0b01110, 0b00101, 0b11110, 0b00100},  // $
  {0b11001, 0b11001, 0b00010, 0b00100, 0b01000, 0b10011, 0b10011},  // %
  {0b01100, 0b10010, 0b10100, 0b01000, 0b10101, 0b10010, 0b01101},  // &
  {0b00100, 0b00100, 0b01000, 0b00000, 0b00000, 0b00000, 0b00000},  // '
  {0b00010, 0b00100, 0b01000, 0b01000, 0b01000, 0b00100, 0b00010},  // (
  {0b01000, 0b00100, 0b00010, 0b00010, 0b00010, 0b00100, 0b01000},  // )
  {0b00000, 0b00100, 0b10101, 0b01110, 0b10101, 0b00100, 0b00000},  // *
  {0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000},  // +
  {0b00000, 0b00000, 0b00000, 0b00000, 0b00100, 0b00100, 0b01000},  // ,
  {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000},  // -
  {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00100},  // .
  {0b00001, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b10000},  // /
  {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},  // 0
  {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},  // 1
  {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},  // 2
  {0b01110, 0b10001, 0b00001, 0b00110, 0b00001, 0b10001, 0b01110},  // 3
  {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},  // 4
  {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110},  // 5
  {0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},  // 6
  {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},  // 7
  {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},  // 8
  {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110},  // 9
  {0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000},  // :
  {0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b01000},  // ;
  {0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010},  // <
  {0b00000, 0b00000, 0b11111, 0b00000, 0b11111, 0b00000, 0b00000},  // =
  {0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000},  // >
  {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b00000, 0b00100},  // ?
  {0b01110, 0b10001, 0b10111, 0b10101, 0b10111, 0b10000, 0b01110},  // @
  {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},  // A
  {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110},  // B
  {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110},  // C
  {0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110},  // D
  {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111},  // E
  {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000},  // F
  {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111},  // G
  {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},  // H
  {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},  // I
  {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100},  // J
  {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001},  // K
  {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111},  // L
  {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001},  // M
  {0b10001, 0b11001, 0b11001, 0b10101, 0b10011, 0b10011, 0b10001},  // N
  {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},  // O
  {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000},  // P
  {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101},  // Q
  {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001},  // R
  {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110},  // S
  {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},  // T
  {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},  // U
  {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100},  // V
  {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001},  // W
  {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001},  // X
  {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100},  // Y
  {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111},  // Z
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
  if (index >= len || cellCol >= GLYPH_COLUMNS) return false;
  return (glyph(text[index])[row] >> (GLYPH_COLUMNS - 1 - cellCol)) & 1;
}

// Draws text with its left edge at text-frame x = left, vertically centered.
// Each LED is either fully on or off (nearest font pixel): the panel has too
// few LEDs for smoothing, which just fattens the strokes.
//
// LED centers sit at half-integer multiples of PIXEL in the tilted text frame,
// so a font pixel's boundaries must fall on whole multiples of PIXEL: `left`
// has to be a multiple of PIXEL, and the rows are shifted half a pixel. If an
// LED sat exactly on a boundary, floating-point noise would decide its pixel.
void draw(const char* text, int len, float left, CRGB color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    led_matrix::Point2D p = led_matrix::screenPos(i);

    // Into the tilted text frame: u runs along the text, v is "up" for the glyphs.
    float u = p.x * TEXT_COS + p.y * TEXT_SIN;
    float v = -p.x * TEXT_SIN + p.y * TEXT_COS;

    int col = (int)floorf((u - left) / PIXEL);
    int row = (int)floorf(GLYPH_ROWS * 0.5f + 0.5f - v / PIXEL);
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
