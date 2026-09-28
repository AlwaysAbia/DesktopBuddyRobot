// Pull-based OTA: fetch a manifest over HTTP(S), compare versions, flash the
// other app slot with Update.h, reboot. Rollback-safe: see ota_update.cpp.
#pragma once

namespace ota {

// Call early in setup(). If this image was just installed by OTA and is in
// PENDING_VERIFY, marks it valid so the bootloader keeps it. If it is never
// called, the next reset rolls back to the previous firmware.
void confirmRunningFirmware();

// Fetch the manifest and, if it offers a newer version, download + flash it
// and reboot (does not return in that case). force = install whatever the
// manifest offers, even if not newer or previously failed.
// Blocks for the duration of the check/download. Needs WiFi.
void checkAndUpdate(bool force);

// Print version, partition and rollback state to Serial.
void printStatus();

}  // namespace ota
