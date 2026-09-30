// ThingsBoard OTA: firmware packages assigned to this device in ThingsBoard are
// downloaded over the MQTT connection (tb_client) and flashed to the other app
// slot. Rollback-safe: see ota_update.cpp.
#pragma once

#include <Arduino.h>

namespace ota {

// Call early in setup(). If this image was just installed by OTA and is in
// PENDING_VERIFY it is NOT marked valid here: it is confirmed by the first
// successful ThingsBoard connection, or rolled back if that takes longer than
// OTA_CONFIRM_TIMEOUT_S (checked in loop()). A crash or reset before that also
// rolls back. Images that are already valid are left alone.
void confirmRunningFirmware();

// Hook into tb_client: report firmware version / update result on connect.
void begin();

// Ask ThingsBoard which package is assigned and install it if it differs from
// the running version. Non-blocking: waits for the connection if needed, the
// rest happens in loop(). force = also install a version that failed to boot before.
// Returns false if a check/update is already in progress (nothing new started).
bool requestCheck(bool force);

// Drives the check. Call every loop, after tb_client::loop().
void loop();

// True while a firmware download/flash is in progress.
bool isUpdating();

// Snapshot of the OTA/rollback state for remote status reporting.
struct Info {
  const char* partition;   // running partition label, e.g. "app0"
  const char* imageState;  // NEW / PENDING_VERIFY / VALID / ... / n/a
  bool rollbackPossible;
  bool awaitingConfirm;    // running unconfirmed, waiting for the first ThingsBoard connection
  String badVersion;       // "" = none
  String lastError;        // "" = none
};
Info info();

// Print version, partition, rollback and ThingsBoard state to Serial.
void printStatus();

}  // namespace ota
