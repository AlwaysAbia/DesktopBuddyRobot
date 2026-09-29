#include "messages.h"

#include <Arduino.h>
#include <Preferences.h>

#include "display_config.h"
#include "led_matrix.h"
#include "led_text.h"

namespace {

// NVS layout (interface-contract.md, section 2): namespace "msgs",
// keys "m0" (newest) .. "m2" (oldest). A missing key means an empty slot.
const char* NVS_NAMESPACE = "msgs";
const char* KEYS[messages::MAX_MESSAGES] = {"m0", "m1", "m2"};

const CRGB MESSAGE_COLOR     = CRGB(200, 200, 200);
const CRGB PLACEHOLDER_COLOR = CRGB(50, 50, 80);

char stored[messages::MAX_MESSAGES][messages::MAX_LENGTH + 1];
int storedCount = 0;

int showing = 0;              // index of the message being scrolled
unsigned long scrollStart = 0;

// True if led_text can draw c (its font covers ' ' to 'Z', lowercase prints as uppercase).
bool drawable(char c) {
  return (c >= ' ' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

void save() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  for (int i = 0; i < messages::MAX_MESSAGES; i++) {
    if (i < storedCount) {
      prefs.putString(KEYS[i], stored[i]);
    } else if (prefs.isKey(KEYS[i])) {
      prefs.remove(KEYS[i]);
    }
  }
  prefs.end();
}

}  // namespace

namespace messages {

void begin() {
  Preferences prefs;
  // Read-write so the namespace is created on first boot (read-only fails if it doesn't exist).
  prefs.begin(NVS_NAMESPACE, false);
  storedCount = 0;
  for (int i = 0; i < MAX_MESSAGES; i++) {
    // Preferences::getString() logs an error for a missing key; check first.
    if (!prefs.isKey(KEYS[i])) break;
    prefs.getString(KEYS[i], stored[storedCount], sizeof(stored[storedCount]));
    storedCount++;
  }
  prefs.end();
  Serial.printf("[Msg] Loaded %d message(s) from NVS\r\n", storedCount);

#if MESSAGES_DEBUG_SEED
  if (storedCount == 0) {
    Serial.println("[Msg] DEBUG: seeding 2 dummy messages");
    add("Hello from BuddyBot");
    add("Test message 2");
  }
#endif
}

void add(const char* text) {
  // Store the text without line breaks (CR / LF); the display has no use for them.
  char clean[MAX_LENGTH + 1];
  int length = 0;
  for (const char* c = text; *c != 0 && length < MAX_LENGTH; c++) {
    if (*c != 13 && *c != 10) clean[length++] = *c;
  }
  clean[length] = 0;

  // Trim both ends: spaces, characters the font can't draw (they would show as
  // "?"), and one pair of surrounding double quotes.
  int start = 0;
  while (start < length && (clean[start] == ' ' || !drawable(clean[start]))) start++;
  while (length > start && (clean[length - 1] == ' ' || !drawable(clean[length - 1]))) length--;
  if (length - start >= 2 && clean[start] == 34 && clean[length - 1] == 34) {
    start++;
    length--;
  }
  if (length - start <= 0) return;  // nothing left to show
  memmove(clean, clean + start, length - start);
  length -= start;
  clean[length] = 0;

  // Shift older messages down; the oldest falls off the end.
  for (int i = MAX_MESSAGES - 1; i > 0; i--) {
    strlcpy(stored[i], stored[i - 1], sizeof(stored[i]));
  }
  strlcpy(stored[0], clean, sizeof(stored[0]));
  if (storedCount < MAX_MESSAGES) storedCount++;
  save();
  onEnter();
}

void clear() {
  storedCount = 0;
  save();
  onEnter();
}

int count() {
  return storedCount;
}

const char* get(int index) {
  return (index >= 0 && index < storedCount) ? stored[index] : "";
}

void onEnter() {
  showing = 0;
  scrollStart = millis();
}

void render() {
  fill_solid(led_matrix::leds, NUM_LEDS, CRGB::Black);

  if (storedCount == 0) {
    if (led_text::drawScrolling("NO MESSAGES YET", scrollStart, PLACEHOLDER_COLOR)) scrollStart = millis();
    return;
  }

  // Newest to oldest, one after another, then repeat.
  if (showing >= storedCount) showing = 0;
  if (led_text::drawScrolling(stored[showing], scrollStart, MESSAGE_COLOR)) {
    showing = (showing + 1) % storedCount;
    scrollStart = millis();
  }
}

}  // namespace messages
