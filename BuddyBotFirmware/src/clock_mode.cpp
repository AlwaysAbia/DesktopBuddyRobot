#include "clock_mode.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

#include "display_config.h"
#include "led_matrix.h"
#include "led_text.h"

namespace {

// Clock face layout, in screen units (panel radius ~0.95).
constexpr float TICK_RADIUS   = 0.85f;
constexpr float HOUR_LENGTH   = 0.45f;
constexpr float MINUTE_LENGTH = 0.80f;
constexpr float HAND_STEP     = 0.05f;   // finer than the LED pitch, so no gaps

const CRGB HOUR_COLOR     = CRGB(255, 110, 0);
const CRGB MINUTE_COLOR   = CRGB(0, 120, 255);
const CRGB SECOND_COLOR   = CRGB(150, 0, 150);
const CRGB TICK_12_COLOR  = CRGB(150, 150, 150);
const CRGB TICK_COLOR     = CRGB(45, 45, 45);
const CRGB NO_TIME_COLOR  = CRGB(160, 60, 0);

bool sntpStarted = false;
volatile bool syncedFlag = false;   // set from the SNTP (lwIP) task
volatile uint32_t syncCount = 0;
uint32_t reportedSyncCount = 0;
unsigned long placeholderStart = 0;

void onTimeSync(struct timeval*) {
  syncCount = syncCount + 1;
  syncedFlag = true;
}

// Index of the LED closest to screen point (x, y).
int nearestLed(float x, float y) {
  int best = 0;
  float bestDist = 1e9f;
  for (int i = 0; i < NUM_LEDS; i++) {
    led_matrix::Point2D p = led_matrix::screenPos(i);
    float d = (p.x - x) * (p.x - x) + (p.y - y) * (p.y - y);
    if (d < bestDist) {
      bestDist = d;
      best = i;
    }
  }
  return best;
}

// Clockwise angle from 12 o'clock, as a fraction of a full turn.
void ringPoint(float turn, float radius, float& x, float& y) {
  float a = turn * 2.0f * M_PI;
  x = sinf(a) * radius;
  y = cosf(a) * radius;
}

// A hand is a chain of single LEDs: step along the hand and light the LED
// nearest each point. Crisp and connected; the panel is too sparse for
// anti-aliased lines (they came out as an unreadable glow).
void drawHand(float turn, float from, float to, CRGB color) {
  for (float r = from; r <= to; r += HAND_STEP) {
    float x, y;
    ringPoint(turn, r, x, y);
    led_matrix::leds[nearestLed(x, y)] |= color;
  }
}

void drawRingDot(float turn, CRGB color) {
  float x, y;
  ringPoint(turn, TICK_RADIUS, x, y);
  led_matrix::leds[nearestLed(x, y)] |= color;
}

void renderFace() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  struct tm local;
  localtime_r(&tv.tv_sec, &local);

  float seconds = local.tm_sec + tv.tv_usec / 1e6f;
  float minutes = local.tm_min + seconds / 60.0f;
  float hours   = (local.tm_hour % 12) + minutes / 60.0f;

  // Hour marks: 12 o'clock brightest, then every hour.
  for (int h = 0; h < 12; h++) drawRingDot(h / 12.0f, h == 0 ? TICK_12_COLOR : TICK_COLOR);
  drawHand(hours / 12.0f, 0.0f, HOUR_LENGTH, HOUR_COLOR);
  drawHand(minutes / 60.0f, 0.0f, MINUTE_LENGTH, MINUTE_COLOR);
  drawRingDot(floorf(seconds) / 60.0f, SECOND_COLOR);
}

}  // namespace

namespace clock_mode {

void loop() {
  if (!sntpStarted && WiFi.status() == WL_CONNECTED) {
    sntp_set_time_sync_notification_cb(&onTimeSync);
    configTzTime(CLOCK_TZ, NTP_SERVER_1, NTP_SERVER_2);
    sntpStarted = true;
    Serial.println("[Time] NTP started");
  }

  uint32_t count = syncCount;
  if (count != reportedSyncCount) {
    reportedSyncCount = count;
    struct tm local;
    time_t now = time(nullptr);
    localtime_r(&now, &local);
    Serial.printf("[Time] NTP sync #%u: %04d-%02d-%02d %02d:%02d:%02d (%s)\r\n", (unsigned)count,
                  local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min,
                  local.tm_sec, CLOCK_TZ);
  }
}

bool synced() {
  return syncedFlag;
}

void onEnter() {
  placeholderStart = millis();
}

void render() {
  fill_solid(led_matrix::leds, NUM_LEDS, CRGB::Black);

  if (synced()) {
    renderFace();
    return;
  }

  // No valid time yet: say why instead of showing a wrong time.
  const char* text = WiFi.status() == WL_CONNECTED ? "SYNCING TIME" : "NEED WIFI";
  if (led_text::drawScrolling(text, placeholderStart, NO_TIME_COLOR)) placeholderStart = millis();
}

}  // namespace clock_mode
