// MESSAGE_HISTORY mode: the last 3 messages, persisted in NVS.
// Nothing adds messages yet; Phase 4 (ThingsBoard messaging) will call add().
#pragma once

namespace messages {

constexpr int MAX_MESSAGES = 3;
constexpr int MAX_LENGTH   = 100;  // characters; longer text is cut off

// Loads the stored messages from NVS. Call once in setup().
void begin();

// Stores text as the newest message, dropping the oldest when full. Saved to NVS.
void add(const char* text);

// Deletes all messages, in RAM and NVS.
void clear();

int count();

// 0 = newest.
const char* get(int index);

// Restarts the scroll from the newest message. Call when the mode becomes active.
void onEnter();

// Draws one frame into led_matrix::leds.
void render();

}  // namespace messages
