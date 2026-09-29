#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>

#include "clock_mode.h"
#include "display_modes.h"
#include "led_matrix.h"
#include "messages.h"
#include "ota_config.h"
#include "ota_update.h"
#include "remote_test.h"
#include "tb_client.h"

// Credentials live in include/secrets.h (gitignored).
// Copy include/secrets.example.h to include/secrets.h and fill it in.
#include "secrets_loader.h"

// ==========================================
// WI-FI CONFIGURATION
// Firmware updates come only from ThingsBoard OTA (src/ota_update.cpp);
// USB upload is the recovery path.
// ==========================================
const char* ssid     = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// ==========================================
// LED PANEL & DISPLAY MODES
// Pin / LED count / brightness / FPS: include/led_matrix.h
// Modes (eye, clock, message history): include/display_modes.h
// ==========================================

// ==========================================
// BUZZER SELF-TEST (Phase 0 bring-up)
// DISABLED: no sound on GPIO32/33/25/26 during bring-up. Buzzer hardware is
// suspect and the actual pin is unconfirmed. Re-enable once that is resolved.
// ==========================================
#define BUZZER_SELFTEST     0
#define BUZZER_PIN          32   // UNCONFIRMED - see note above
#define BUZZER_ACTIVE_HIGH  1    // Active buzzer driven directly / via NPN. Set 0 for active-low (PNP) modules.

// Helper function to translate status codes into text
const char* getWiFiStatusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:     return "IDLE_STATUS";
    case WL_NO_SSID_AVAIL:   return "NO_SSID_AVAIL (SSID not found)";
    case WL_SCAN_COMPLETED:  return "SCAN_COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECT_FAILED (Wrong password/Security mismatch)";
    case WL_CONNECTION_LOST: return "CONNECTION_LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

// ==========================================
// WI-FI INITIALIZATION WITH LOGGING
// ==========================================
void setupWiFi() {
  // 1. Fully disconnect and clear lingering state
  WiFi.disconnect(true);
  delay(100);

  // 2. Configure Station Mode & Disable Modem Sleep
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Prevents dropouts on Android hotspots

  Serial.print("[WiFi] Attempting connection to SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // 3. Connection Handshake Loop with Detailed Debug Prints
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { // 20-second window
    delay(500);
    Serial.print(".");
    
    // Print current status code every 5 seconds
    if (attempts > 0 && attempts % 10 == 0) {
      Serial.printf("\r\n[WiFi Handshake Status]: %s\r\n", getWiFiStatusName(WiFi.status()));
    }
    attempts++;
  }

  // 4. Verify Connection Result
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\r\n\r\n[WiFi] CONNECTION SUCCESSFUL!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\r\n\r\n[WiFi] CONNECTION FAILED!");
    Serial.printf("[WiFi] Final Reason: %s\r\n", getWiFiStatusName(WiFi.status()));
    Serial.println("[WiFi] Proceeding to run animation in OFFLINE mode.");
  }
  Serial.println("==========================================\r\n");
}

#if BUZZER_SELFTEST
// 3 short beeps. Active buzzer: plain on/off, no PWM.
void buzzerSelfTest() {
  const uint8_t onLevel  = BUZZER_ACTIVE_HIGH ? HIGH : LOW;
  const uint8_t offLevel = BUZZER_ACTIVE_HIGH ? LOW : HIGH;

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, offLevel);

  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, onLevel);
    delay(80);
    digitalWrite(BUZZER_PIN, offLevel);
    delay(120);
  }
}
#endif

// ==========================================
// SERIAL COMMANDS
//   ota        - install the package assigned in ThingsBoard if it differs
//   ota force  - same, even if that version failed to boot before
//   status     - print version / partition / rollback / ThingsBoard state
//   mode       - TEMPORARY: switch to the next display mode
//   msg clear  - TEMPORARY: delete the stored message history
// The two TEMPORARY commands are test placeholders until BLE control (next
// session) and Phase 4 messaging exist; remove them then. The same controls
// are available remotely as the RPCs in remote_test.cpp.
// ==========================================
void handleSerialCommands() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c != '\n' && c != '\r') {
      if (line.length() < 32) line += c;
      continue;
    }
    line.trim();
    if (line == "ota")            ota::requestCheck(false);
    else if (line == "ota force") ota::requestCheck(true);
    else if (line == "status")    ota::printStatus();
    else if (line == "mode")      modes::next();          // TEMPORARY, see above
    else if (line == "msg clear") messages::clear();      // TEMPORARY, see above
    else if (line.length())       Serial.println("Commands: ota | ota force | status | mode | msg clear");
    line = "";
  }
}

// ==========================================
// MAIN SETUP & LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\r\n==========================================");
  Serial.printf("   ESP32 Sci-Fi Robot Eye Booting (%s %s)\r\n", FIRMWARE_TITLE, FIRMWARE_VERSION);
  Serial.println("==========================================");

  // Initialize FastLED + coordinate map (LEDs remain OFF during WiFi setup to prevent brownout)
  led_matrix::begin();

#if BUZZER_SELFTEST
  buzzerSelfTest();
#endif

  // Rollback safety: an image freshly installed by OTA (either path) must
  // confirm itself, or the bootloader reverts it on the next reset.
  // Keep this early in setup().
  ota::confirmRunningFirmware();
  ota::printStatus();

  // Display modes (boots into EYE_ANIMATION) + stored message history
  messages::begin();
  modes::begin();

  // Initialize WiFi
  setupWiFi();

  // ThingsBoard connects from loop(); the boot check waits for it.
  ota::begin();
  remote_test::begin();  // TEMPORARY mode/message test RPCs
#if OTA_CHECK_ON_BOOT
  ota::requestCheck(false);
#endif
}

void loop() {
  // ThingsBoard connection + OTA check
  tb_client::loop();
  ota::loop();
  clock_mode::loop();  // starts NTP once WiFi is up
  remote_test::loop();
  handleSerialCommands();

  // While a ThingsBoard firmware download is running, keep the eye dark
  // (lower current while flash is written) and skip the frame delay so chunks
  // are requested back-to-back.
  static bool blanked = false;
  if (ota::isUpdating()) {
    if (!blanked) {
      FastLED.clear(true);
      blanked = true;
    }
    delay(1);
    return;
  }
  blanked = false;

  // Run render pipeline for the current mode
  modes::render();

  FastLED.show();
  FastLED.delay(1000 / FPS);
}
