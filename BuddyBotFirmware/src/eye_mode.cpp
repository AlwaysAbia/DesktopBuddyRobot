#include "eye_mode.h"

#include <Arduino.h>
#include <math.h>

#include "led_matrix.h"

using led_matrix::leds;
using led_matrix::ledCoords;

namespace {

// ==========================================
// COLOR PALETTES & ANIMATION STATES
// ==========================================
struct EyePalette {
  CRGB irisCore;
  CRGB irisEdge;
  CRGB eyelidEdge;
};

// Indexed by eye::Theme.
const EyePalette THEMES[eye::THEME_COUNT] = {
  { CRGB(0, 255, 255), CRGB(0, 30, 120), CRGB(180, 255, 255) }, // Cyan
  { CRGB(255, 140, 0), CRGB(120, 20, 0),  CRGB(255, 200, 100) }, // Amber
  { CRGB(0, 255, 100), CRGB(0, 80, 20),   CRGB(150, 255, 180) }, // Emerald
  { CRGB(255, 0, 150), CRGB(80, 0, 80),   CRGB(255, 160, 220) }  // Magenta
};
uint8_t currentTheme = eye::THEME_AMBER;

float currentPupilX = 0.0f, currentPupilY = 0.0f;
float targetPupilX  = 0.0f, targetPupilY  = 0.0f;

unsigned long lastGazeChange = 0;
unsigned long gazeInterval   = 2000;

enum BlinkState { IDLE, CLOSING, CLOSED, OPENING };
BlinkState blinkState = IDLE;
float blinkProgress   = 0.0f;
unsigned long lastBlinkCheck = 0;
unsigned long nextBlinkTime  = 3000;

void updateGazeTarget() {
  if (millis() - lastGazeChange > gazeInterval) {
    float angle = (random(0, 360) * M_PI) / 180.0f;
    float dist  = (random(0, 100) / 100.0f) * 0.40f;

    targetPupilX = cosf(angle) * dist;
    targetPupilY = sinf(angle) * dist;

    gazeInterval   = random(1200, 3500);
    lastGazeChange = millis();
  }

  currentPupilX += (targetPupilX - currentPupilX) * 0.08f;
  currentPupilY += (targetPupilY - currentPupilY) * 0.08f;
}

void updateBlinkAnimation() {
  unsigned long now = millis();

  switch (blinkState) {
    case IDLE:
      if (now - lastBlinkCheck > nextBlinkTime) {
        blinkState    = CLOSING;
        lastBlinkCheck = now;
      }
      break;

    case CLOSING:
      blinkProgress += 0.12f;
      if (blinkProgress >= 1.0f) {
        blinkProgress = 1.0f;
        blinkState    = CLOSED;
        lastBlinkCheck = now;
      }
      break;

    case CLOSED:
      if (now - lastBlinkCheck > 60) {
        blinkState = OPENING;
      }
      break;

    case OPENING:
      blinkProgress -= 0.10f;
      if (blinkProgress <= 0.0f) {
        blinkProgress  = 0.0f;
        blinkState     = IDLE;
        lastBlinkCheck = now;
        nextBlinkTime  = random(2000, 6000);
      }
      break;
  }
}

void renderEyeFrame() {
  const EyePalette& theme = THEMES[currentTheme];

  const float pupilRadius = 0.28f;
  const float irisRadius  = 1.05f;

  const float glint1X = currentPupilX + 0.10f;
  const float glint1Y = currentPupilY + 0.10f;
  const float glint2X = currentPupilX - 0.08f;
  const float glint2Y = currentPupilY - 0.08f;

  float eyelidCutoffY = (1.0f - blinkProgress) * 0.95f;

  // The primary glint is always the LED nearest its ideal spot. A radius test
  // (the old way) often contained no LED at all and the glint vanished.
  int glintLed = 0;
  float glintBest = 1e9f;
  for (int i = 0; i < NUM_LEDS; i++) {
    float dx = ledCoords[i].x - glint1X;
    float dy = ledCoords[i].y - glint1Y;
    float d = dx * dx + dy * dy;
    if (d < glintBest) {
      glintBest = d;
      glintLed = i;
    }
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    float x = ledCoords[i].x;
    float y = ledCoords[i].y;

    // Eyelid horizontal cutoff
    float absY = fabsf(y);
    if (absY >= eyelidCutoffY) {
      leds[i] = CRGB::Black;

      if (absY < eyelidCutoffY + 0.20f && blinkProgress > 0.05f) {
        leds[i] = theme.eyelidEdge;
        leds[i].nscale8(200);
      }
      continue;
    }

    if (blinkProgress >= 0.92f && absY < 0.18f) {
      leds[i] = theme.eyelidEdge;
      continue;
    }

    // Radial rendering
    float dx = x - currentPupilX;
    float dy = y - currentPupilY;
    float distToPupil = sqrtf(dx * dx + dy * dy);

    // Primary Specular Glint
    if (i == glintLed) {
      leds[i] = CRGB::White;
      continue;
    }

    // Secondary Specular Glint
    float dGlint2 = sqrtf((x - glint2X) * (x - glint2X) + (y - glint2Y) * (y - glint2Y));
    if (dGlint2 < 0.05f) {
      leds[i] = CRGB(180, 180, 180);
      continue;
    }

    // Pupil
    if (distToPupil <= pupilRadius) {
      leds[i] = CRGB::Black;
    }
    // Iris
    else if (distToPupil <= irisRadius) {
      float t = (distToPupil - pupilRadius) / (irisRadius - pupilRadius);
      t = constrain(t, 0.0f, 1.0f);

      CRGB color = blend(theme.irisCore, theme.irisEdge, uint8_t(t * 255.0f));

      if (t > 0.85f) {
        float rimFactor = (1.0f - t) / 0.15f;
        color.nscale8(uint8_t(rimFactor * 255.0f));
      }

      leds[i] = color;
    }
    else {
      leds[i] = CRGB::Black;
    }
  }
}

}  // namespace

namespace eye {

void setTheme(uint8_t theme) {
  if (theme < THEME_COUNT) currentTheme = theme;
}

uint8_t theme() {
  return currentTheme;
}

void render() {
  updateGazeTarget();
  updateBlinkAnimation();
  renderEyeFrame();
}

}  // namespace eye
