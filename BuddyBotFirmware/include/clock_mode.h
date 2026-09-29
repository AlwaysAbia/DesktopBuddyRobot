// CURRENT_TIME mode: NTP time sync + an analog clock face.
// After the first sync the ESP32's own system clock keeps time; SNTP re-syncs
// it in the background (hourly, the ESP-IDF default).
#pragma once

namespace clock_mode {

// Starts SNTP the first time WiFi is connected. Call every loop.
void loop();

// True once an NTP sync has succeeded since boot.
bool synced();

// Restarts the placeholder text scroll. Call when the mode becomes active.
void onEnter();

// Draws one frame into led_matrix::leds.
void render();

}  // namespace clock_mode
