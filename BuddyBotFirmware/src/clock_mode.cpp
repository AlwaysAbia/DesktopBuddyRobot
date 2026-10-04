#include "clock_mode.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>

#include "display_config.h"
#include "led_matrix.h"
#include "led_text.h"

namespace {

const CRGB NO_TIME_COLOR  = CRGB(160, 60, 0);

const CRGB TIME_COLOR = CRGB(0, 90, 200);

bool sntpStarted = false;
volatile bool syncedFlag = false;   // set from the SNTP (lwIP) task
volatile uint32_t syncCount = 0;
uint32_t reportedSyncCount = 0;
unsigned long placeholderStart = 0;

// Persisted so the clock still runs (approximately) when WiFi never comes up.
const char* NVS_NAMESPACE = "time";
const char* KEY_LAST      = "last";
constexpr time_t MIN_VALID_TIME = 1700000000;    // Nov 2023: anything earlier is "clock never set"
constexpr unsigned long SAVE_INTERVAL_MS = 30UL * 60 * 1000;
bool restoredFromNvs = false;
unsigned long lastSave = 0;

void saveTime() {
  time_t now = time(nullptr);
  if (now < MIN_VALID_TIME) return;
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putULong64(KEY_LAST, (uint64_t)now);
  prefs.end();
  lastSave = millis();
}

void onTimeSync(struct timeval*) {
  syncCount = syncCount + 1;
  syncedFlag = true;
}

}  // namespace

namespace clock_mode {

void begin() {
  setenv("TZ", CLOCK_TZ, 1);
  tzset();
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  uint64_t last = prefs.isKey(KEY_LAST) ? prefs.getULong64(KEY_LAST, 0) : 0;
  prefs.end();
  if (last >= (uint64_t)MIN_VALID_TIME) {
    struct timeval tv = {(time_t)last, 0};
    settimeofday(&tv, nullptr);
    restoredFromNvs = true;
    Serial.printf("[Time] Restored %lu from NVS (approximate until NTP syncs)\r\n", (unsigned long)last);
  } else {
    Serial.println("[Time] No stored time");
  }
}

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
    saveTime();
  }

  if (hasTime() && millis() - lastSave > SAVE_INTERVAL_MS) saveTime();
}

bool synced() {
  return syncedFlag;
}

bool hasTime() {
  return syncedFlag || restoredFromNvs;
}

void onEnter() {
  placeholderStart = millis();
}

void render() {
  fill_solid(led_matrix::leds, NUM_LEDS, CRGB::Black);

  char text[12];
  CRGB color = TIME_COLOR;

  if (hasTime()) {
    time_t now = time(nullptr);
    struct tm local;
    localtime_r(&now, &local);
    snprintf(text, sizeof(text), "%02d:%02d", local.tm_hour, local.tm_min);
  } else {
    // No valid time yet: say why instead of showing a wrong time.
    strcpy(text, WiFi.status() == WL_CONNECTED ? "SYNCING TIME" : "NEED WIFI");
    color = NO_TIME_COLOR;
  }

  if (led_text::drawScrolling(text, placeholderStart, color)) placeholderStart = millis();
}

}  // namespace clock_mode
