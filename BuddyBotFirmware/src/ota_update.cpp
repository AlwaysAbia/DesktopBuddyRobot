#include "ota_update.h"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_ota_ops.h>

#include "ota_config.h"

// ==========================================
// ROLLBACK
// The prebuilt arduino-esp32 core has CONFIG_APP_ROLLBACK_ENABLE=y, but by
// default initArduino() marks a freshly OTA'd image valid before setup() even
// runs, which makes rollback useless. Overriding this weak hook to return true
// hands that decision to us: ota::confirmRunningFirmware().
// Applies to every OTA path (this pull OTA and ArduinoOTA pushes alike).
// ==========================================
extern "C" bool verifyRollbackLater() {
  return true;
}

// Mozilla root CA bundle compiled into the core (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE).
extern const uint8_t ca_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t ca_bundle_end[]   asm("_binary_x509_crt_bundle_end");

namespace {

// NVS bookkeeping so a rolled-back version isn't re-downloaded forever.
// Keys are listed in interface-contract.md, section 2.
const char* NVS_NAMESPACE = "ota";
const char* KEY_PENDING   = "pending";  // version we just flashed, cleared on next boot
const char* KEY_BAD       = "bad";      // version that failed to boot (rolled back)

// Preferences::getString() logs an error for a missing key; check first.
String readKey(Preferences& prefs, const char* key) {
  return prefs.isKey(key) ? prefs.getString(key, "") : String();
}

struct Manifest {
  String version;
  String url;
  String md5;  // optional
};

const char* stateName(esp_ota_img_states_t state) {
  switch (state) {
    case ESP_OTA_IMG_NEW:            return "NEW";
    case ESP_OTA_IMG_PENDING_VERIFY: return "PENDING_VERIFY";
    case ESP_OTA_IMG_VALID:          return "VALID";
    case ESP_OTA_IMG_INVALID:        return "INVALID";
    case ESP_OTA_IMG_ABORTED:        return "ABORTED";
    case ESP_OTA_IMG_UNDEFINED:      return "UNDEFINED";
    default:                         return "UNKNOWN";
  }
}

// Parses "1.2.3" or "v1.2.3"; missing parts count as 0.
bool parseVersion(const String& text, long out[3]) {
  const char* p = text.c_str();
  if (*p == 'v' || *p == 'V') p++;
  for (int i = 0; i < 3; i++) {
    out[i] = 0;
    if (*p == '\0') continue;
    if (!isdigit((unsigned char)*p)) return false;
    char* end;
    out[i] = strtol(p, &end, 10);
    p = end;
    if (*p == '.') p++;
    else if (*p != '\0') return false;
  }
  return *p == '\0';
}

// <0 if a<b, 0 if equal, >0 if a>b.
int compareVersions(const long a[3], const long b[3]) {
  for (int i = 0; i < 3; i++) {
    if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
  }
  return 0;
}

// HTTPClient borrows the network client, so the caller owns both.
// Without our own secure client, HTTPClient::begin(url) silently uses setInsecure().
bool httpBegin(HTTPClient& http, NetworkClient& plain, NetworkClientSecure& secure, const String& url) {
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);
  if (url.startsWith("https://")) {
    secure.setCACertBundle(ca_bundle_start, ca_bundle_end - ca_bundle_start);
    return http.begin(secure, url);
  }
  return http.begin(plain, url);
}

bool fetchManifest(Manifest& manifest) {
  NetworkClient plain;
  NetworkClientSecure secure;
  HTTPClient http;

  Serial.printf("[OTA] Fetching manifest: %s\r\n", OTA_MANIFEST_URL);
  if (!httpBegin(http, plain, secure, OTA_MANIFEST_URL)) {
    Serial.println("[OTA] Invalid manifest URL");
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] Manifest request failed: %d (%s)\r\n", code, http.errorToString(code).c_str());
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[OTA] Manifest is not valid JSON: %s\r\n", err.c_str());
    return false;
  }

  manifest.version = doc["version"] | "";
  manifest.url     = doc["url"] | "";
  manifest.md5     = doc["md5"] | "";
  if (manifest.version.isEmpty() || manifest.url.isEmpty()) {
    Serial.println("[OTA] Manifest must contain \"version\" and \"url\"");
    return false;
  }
  return true;
}

bool downloadAndFlash(const Manifest& manifest) {
  NetworkClient plain;
  NetworkClientSecure secure;
  HTTPClient http;

  // HTTP/1.0 = no chunked transfer encoding, so the stream is the raw binary.
  http.useHTTP10(true);

  Serial.printf("[OTA] Downloading: %s\r\n", manifest.url.c_str());
  if (!httpBegin(http, plain, secure, manifest.url)) {
    Serial.println("[OTA] Invalid firmware URL");
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] Firmware request failed: %d (%s)\r\n", code, http.errorToString(code).c_str());
    http.end();
    return false;
  }

  int length = http.getSize();
  bool sizeKnown = length > 0;
  if (!Update.begin(sizeKnown ? (size_t)length : UPDATE_SIZE_UNKNOWN, U_FLASH)) {
    Serial.printf("[OTA] Update.begin failed: %s\r\n", Update.errorString());
    http.end();
    return false;
  }
  if (!manifest.md5.isEmpty() && !Update.setMD5(manifest.md5.c_str())) {
    Serial.println("[OTA] Manifest md5 is not a 32-char hex string");
    Update.abort();
    http.end();
    return false;
  }

  int lastDecile = -1;
  Update.onProgress([&lastDecile](size_t done, size_t total) {
    if (total == 0 || total == UPDATE_SIZE_UNKNOWN) return;
    int decile = (int)(done * 10 / total);
    if (decile != lastDecile) {
      lastDecile = decile;
      Serial.printf("[OTA] Progress: %d%%\r\n", decile * 10);
    }
  });

  size_t written = Update.writeStream(*http.getStreamPtr());
  http.end();
  Serial.printf("[OTA] Wrote %u bytes\r\n", (unsigned)written);

  // With a known size, end(false) fails on a truncated download.
  // Also verifies the image header and (if given) the md5, then sets the boot partition.
  if (!Update.end(!sizeKnown)) {
    Serial.printf("[OTA] Update failed: %s\r\n", Update.errorString());
    Update.abort();
    return false;
  }
  return true;
}

}  // namespace

namespace ota {

void confirmRunningFirmware() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  bool hasState = esp_ota_get_state_partition(running, &state) == ESP_OK;

  if (hasState && state == ESP_OTA_IMG_PENDING_VERIFY) {
#if OTA_TEST_CRASH_BEFORE_VALID
    Serial.println("[OTA] TEST: crashing before marking image valid - expect rollback");
    Serial.flush();
    abort();
#endif
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      Serial.printf("[OTA] New firmware %s marked valid (rollback cancelled)\r\n", FIRMWARE_VERSION);
    } else {
      Serial.println("[OTA] ERROR: could not mark firmware valid");
    }
  }

  // Did the last pull OTA we started actually end up running?
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  String pending = readKey(prefs, KEY_PENDING);
  if (!pending.isEmpty()) {
    if (pending == FIRMWARE_VERSION) {
      Serial.printf("[OTA] Update to %s succeeded\r\n", pending.c_str());
      if (prefs.isKey(KEY_BAD)) prefs.remove(KEY_BAD);
    } else {
      // Rolled back (or power was lost before reboot). Don't auto-retry it.
      Serial.printf("[OTA] Update to %s did not stick - still on %s. Marking %s as bad "
                    "(use 'ota force' to retry)\r\n",
                    pending.c_str(), FIRMWARE_VERSION, pending.c_str());
      prefs.putString(KEY_BAD, pending);
    }
    prefs.remove(KEY_PENDING);
  }
  prefs.end();
}

void checkAndUpdate(bool force) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OTA] Skipping check: WiFi not connected");
    return;
  }

  Manifest manifest;
  if (!fetchManifest(manifest)) return;

  long current[3], offered[3];
  parseVersion(FIRMWARE_VERSION, current);
  if (!parseVersion(manifest.version, offered)) {
    Serial.printf("[OTA] Manifest version \"%s\" is not MAJOR.MINOR.PATCH\r\n", manifest.version.c_str());
    return;
  }
  Serial.printf("[OTA] Running %s, manifest offers %s\r\n", FIRMWARE_VERSION, manifest.version.c_str());

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  String bad = readKey(prefs, KEY_BAD);

  if (!force) {
    if (compareVersions(offered, current) <= 0) {
      Serial.println("[OTA] Already up to date");
      prefs.end();
      return;
    }
    if (bad == manifest.version) {
      Serial.printf("[OTA] %s failed to boot before - skipping (use 'ota force' to retry)\r\n", bad.c_str());
      prefs.end();
      return;
    }
  }

  prefs.putString(KEY_PENDING, manifest.version);
  if (!downloadAndFlash(manifest)) {
    prefs.remove(KEY_PENDING);
    prefs.end();
    return;
  }
  prefs.end();

  Serial.printf("[OTA] Flashed %s - rebooting\r\n", manifest.version.c_str());
  Serial.flush();
  delay(200);
  ESP.restart();
}

void printStatus() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);
  esp_ota_img_states_t state;
  bool hasState = esp_ota_get_state_partition(running, &state) == ESP_OK;

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);
  String bad = readKey(prefs, KEY_BAD);
  prefs.end();

  Serial.println("---------- OTA status ----------");
  Serial.printf("Firmware version : %s\r\n", FIRMWARE_VERSION);
  Serial.printf("Running partition: %s @ 0x%06x (state: %s)\r\n", running->label, (unsigned)running->address,
                hasState ? stateName(state) : "n/a");
  Serial.printf("Next OTA slot    : %s\r\n", next ? next->label : "none");
  Serial.printf("Rollback possible: %s\r\n", esp_ota_check_rollback_is_possible() ? "yes" : "no");
  Serial.printf("Bad version      : %s\r\n", bad.isEmpty() ? "-" : bad.c_str());
  Serial.printf("Manifest URL     : %s\r\n", OTA_MANIFEST_URL);
  Serial.println("--------------------------------");
}

}  // namespace ota
