#include "wifi_manager.h"

#include <Preferences.h>
#include <WiFi.h>
#include <string.h>

#include "ble_config.h"
#include "secrets_loader.h"

namespace {

const char* NVS_NAMESPACE = "wifi";
const char* KEY_SSID      = "ssid";
const char* KEY_PASS      = "pass";

constexpr unsigned long CONNECT_WINDOW_MS = 20000;  // no link after this long = FAILED
constexpr unsigned long RETRY_INTERVAL_MS = 30000;  // keep trying while not connected

char ssidBuf[33] = "";
char passBuf[65] = "";

unsigned long attemptStart = 0;   // start of the current "CONNECTING" window
unsigned long lastBegin = 0;
bool failed = false;
bool wasConnected = false;
bool bleConnected = false;
volatile uint8_t lastReason = 0;  // WIFI_REASON_* of the latest disconnect (event task)

// Credentials queued from another task, applied in loop().
portMUX_TYPE pendingMux = portMUX_INITIALIZER_UNLOCKED;
struct {
  bool ready;
  char ssid[33];
  char pass[65];
} pending = {false, "", ""};

bool validPassword(const char* pass) {
  size_t n = strlen(pass);
  return n == 0 || (n >= 8 && n <= 63) || n == 64;
}

void save() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(KEY_SSID, ssidBuf);
  prefs.putString(KEY_PASS, passBuf);
  prefs.end();
}

void startConnect() {
  attemptStart = millis();
  lastBegin = attemptStart;
  if (ssidBuf[0] == '\0') return;
  WiFi.disconnect(false);
  WiFi.begin(ssidBuf, passBuf[0] ? passBuf : nullptr);
  Serial.printf("[WiFi] Connecting to \"%s\" in the background\r\n", ssidBuf);
}

void onDisconnected(arduino_event_id_t, arduino_event_info_t info) {
  lastReason = info.wifi_sta_disconnected.reason;
}

}  // namespace

namespace wifi_mgr {

void begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // prevents dropouts on Android hotspots; see setBleConnected()
  WiFi.setAutoReconnect(true);
  WiFi.onEvent(onDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  if (prefs.isKey(KEY_SSID)) {
    prefs.getString(KEY_SSID, ssidBuf, sizeof(ssidBuf));
    prefs.getString(KEY_PASS, passBuf, sizeof(passBuf));
    Serial.printf("[WiFi] Stored network: %s\r\n", ssidBuf[0] ? ssidBuf : "(none)");
  } else {
    // First-ever boot: start from the build-time credentials (may be empty).
    strlcpy(ssidBuf, WIFI_SSID, sizeof(ssidBuf));
    strlcpy(passBuf, WIFI_PASSWORD, sizeof(passBuf));
    prefs.putString(KEY_SSID, ssidBuf);
    prefs.putString(KEY_PASS, passBuf);
    Serial.printf("[WiFi] First boot, seeded from secrets.h: %s\r\n", ssidBuf[0] ? ssidBuf : "(none)");
  }
  prefs.end();

  if (ssidBuf[0] == '\0') Serial.println("[WiFi] No credentials - waiting for BLE provisioning");
  startConnect();
}

void loop() {
  bool apply = false;
  char newSsid[33], newPass[65];
  portENTER_CRITICAL(&pendingMux);
  if (pending.ready) {
    memcpy(newSsid, pending.ssid, sizeof(newSsid));
    memcpy(newPass, pending.pass, sizeof(newPass));
    pending.ready = false;
    apply = true;
  }
  portEXIT_CRITICAL(&pendingMux);

  if (apply) {
    strlcpy(ssidBuf, newSsid, sizeof(ssidBuf));
    strlcpy(passBuf, newPass, sizeof(passBuf));
    save();
    failed = false;
    wasConnected = false;
    lastReason = 0;
    Serial.printf("[WiFi] New credentials stored: %s\r\n", ssidBuf[0] ? ssidBuf : "(forgotten)");
    if (ssidBuf[0] == '\0') WiFi.disconnect(false);
    startConnect();
    return;
  }

  if (ssidBuf[0] == '\0') return;

  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      Serial.printf("[WiFi] Connected, IP %s, RSSI %d dBm\r\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    wasConnected = true;
    failed = false;
    return;
  }

  if (wasConnected) {  // link just dropped: give it a fresh window
    wasConnected = false;
    attemptStart = millis();
    Serial.println("[WiFi] Connection lost");
  }
  if (!failed && millis() - attemptStart > CONNECT_WINDOW_MS) {
    failed = true;
    Serial.printf("[WiFi] Not connected after %lu s (%s)\r\n", CONNECT_WINDOW_MS / 1000, failReason());
  }
  if (millis() - lastBegin > RETRY_INTERVAL_MS) {
    lastBegin = millis();
    WiFi.disconnect(false);
    WiFi.begin(ssidBuf, passBuf[0] ? passBuf : nullptr);
  }
}

bool queueCredentials(const char* ssid, const char* password) {
  if (ssid == nullptr || password == nullptr) return false;
  if (strlen(ssid) > 32 || !validPassword(password)) return false;
  portENTER_CRITICAL(&pendingMux);
  strlcpy(pending.ssid, ssid, sizeof(pending.ssid));
  strlcpy(pending.pass, password, sizeof(pending.pass));
  pending.ready = true;
  portEXIT_CRITICAL(&pendingMux);
  return true;
}

State state() {
  if (ssidBuf[0] == '\0') return State::NO_CREDENTIALS;
  if (WiFi.status() == WL_CONNECTED) return State::CONNECTED;
  return failed ? State::FAILED : State::CONNECTING;
}

const char* stateName(State s) {
  switch (s) {
    case State::NO_CREDENTIALS: return "none";
    case State::CONNECTING:     return "connecting";
    case State::CONNECTED:      return "connected";
    case State::FAILED:         return "failed";
  }
  return "unknown";
}

const char* failReason() {
  if (state() != State::FAILED) return "";
  switch (lastReason) {
    case WIFI_REASON_NO_AP_FOUND:
      return "not_found";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
      return "auth";
    default:
      return "other";
  }
}

String ssid() {
  return String(ssidBuf);
}

void setBleConnected(bool connected) {
  if (connected == bleConnected) return;
  bleConnected = connected;
#if BLE_WIFI_SLEEP_WHILE_CONNECTED
  WiFi.setSleep(connected);
  Serial.printf("[WiFi] Modem sleep %s (BLE %s)\r\n", connected ? "on" : "off", connected ? "connected" : "idle");
#endif
}

}  // namespace wifi_mgr
