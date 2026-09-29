#include <Arduino.h>
#include <FastLED.h>

#include "ble_control.h"
#include "clock_mode.h"
#include "display_modes.h"
#include "eye_mode.h"
#include "led_matrix.h"
#include "messages.h"
#include "ota_config.h"
#include "ota_update.h"
#include "remote_test.h"
#include "tb_client.h"
#include "wifi_manager.h"

// ==========================================
// WI-FI + BLE
// WiFi credentials come from NVS and connect in the background (src/wifi_manager.cpp);
// the app sets them over BLE (src/ble_control.cpp). Firmware updates come only from
// ThingsBoard OTA (src/ota_update.cpp); USB upload is the recovery path.
// ==========================================

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
//   msg clear  - TEMPORARY: delete the stored message history
// "msg clear" is a TEMPORARY test placeholder until Phase 4 messaging exists.
// It and the mode switch are also available remotely as the RPCs in remote_test.cpp;
// the app switches modes over BLE.
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
    else if (line == "msg clear") messages::clear();      // TEMPORARY, see above
    else if (line.length())       Serial.println("Commands: ota | ota force | status | msg clear");
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

  // Initialize FastLED + coordinate map
  led_matrix::begin();

#if BUZZER_SELFTEST
  buzzerSelfTest();
#endif

  // Rollback safety: an image freshly installed by OTA (either path) must
  // confirm itself, or the bootloader reverts it on the next reset.
  // Keep this early in setup().
  ota::confirmRunningFirmware();
  ota::printStatus();

  // Restore the saved eye color, clock time, message history and display mode.
  eye::begin();
  clock_mode::begin();
  messages::begin();
  modes::begin();

  // WiFi connects in the background: the eye is up right away, with or without WiFi.
  // BLE (mode, color, WiFi provisioning) works either way.
  wifi_mgr::begin();
  ble_control::begin();

  // ThingsBoard connects from loop(); the boot check waits for it.
  ota::begin();
  remote_test::begin();  // TEMPORARY mode/message test RPCs
#if OTA_CHECK_ON_BOOT
  ota::requestCheck(false);
#endif
}

void loop() {
  wifi_mgr::loop();
  ble_control::loop();  // also during OTA downloads, so the app stays responsive

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
