// WiFi station manager: credentials in NVS, connection in the background.
// Nothing here blocks: the eye and BLE run while WiFi connects (or never does).
#pragma once

#include <Arduino.h>

namespace wifi_mgr {

enum class State : uint8_t { NO_CREDENTIALS = 0, CONNECTING = 1, CONNECTED = 2, FAILED = 3 };

// Loads credentials from NVS and starts connecting. On a genuinely first-ever
// boot (no NVS key yet) the WIFI_SSID / WIFI_PASSWORD from secrets.h are stored
// as the starting credentials; after that NVS is the only source.
void begin();

// Applies queued credentials, retries a failed connection. Call every loop.
void loop();

// Validates and queues new credentials; loop() stores them and reconnects.
// Safe to call from any task (the BLE callback). An empty ssid means "forget
// the network". Returns false (and queues nothing) if the values are invalid:
// ssid > 32 bytes, or a password that is not empty (open network), 8..63 chars
// or 64 hex chars.
bool queueCredentials(const char* ssid, const char* password);

State state();
const char* stateName(State s);

// Why the last attempt failed while state() is FAILED: "not_found", "auth" or "other"; "" otherwise.
const char* failReason();

// The stored SSID ("" if none). Never exposes the password.
String ssid();

// While a BLE client is connected WiFi uses modem sleep so the shared radio can
// serve BLE; otherwise it stays awake (Android hotspots drop idle stations).
void setBleConnected(bool connected);

}  // namespace wifi_mgr
