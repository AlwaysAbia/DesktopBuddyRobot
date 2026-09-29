// CURRENT_TIME mode: NTP time sync + an analog clock face.
// After the first sync the ESP32's own system clock keeps time; SNTP re-syncs
// it in the background (hourly, the ESP-IDF default).
#pragma once

namespace clock_mode {

// Restores the last known time from NVS (time/last) so the clock runs offline.
// Call once in setup(), before WiFi is up.
void begin();

// Starts SNTP the first time WiFi is connected and saves the time to NVS.
// Call every loop.
void loop();

// True once an NTP sync has succeeded since boot.
bool synced();

// True if the clock shows a usable time: NTP-synced, or restored from NVS
// (then only approximate: unpowered time is not counted until the next sync).
bool hasTime();

// Restarts the placeholder text scroll. Call when the mode becomes active.
void onEnter();

// Draws one frame into led_matrix::leds.
void render();

}  // namespace clock_mode
