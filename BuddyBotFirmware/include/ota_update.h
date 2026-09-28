// ThingsBoard OTA: firmware packages assigned to this device in ThingsBoard are
// downloaded over the MQTT connection (tb_client) and flashed to the other app
// slot. Rollback-safe: see ota_update.cpp.
#pragma once

namespace ota {

// Call early in setup(). If this image was just installed by OTA and is in
// PENDING_VERIFY, marks it valid so the bootloader keeps it. If it is never
// called, the next reset rolls back to the previous firmware.
void confirmRunningFirmware();

// Hook into tb_client: report firmware version / update result on connect.
void begin();

// Ask ThingsBoard which package is assigned and install it if it differs from
// the running version. Non-blocking: waits for the connection if needed, the
// rest happens in loop(). force = also install a version that failed to boot before.
void requestCheck(bool force);

// Drives the check. Call every loop, after tb_client::loop().
void loop();

// True while a firmware download/flash is in progress.
bool isUpdating();

// Print version, partition, rollback and ThingsBoard state to Serial.
void printStatus();

}  // namespace ota
