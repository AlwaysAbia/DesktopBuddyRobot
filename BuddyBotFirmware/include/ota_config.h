// Firmware identity + OTA settings. This is the file you touch to cut a new
// OTA build: bump FIRMWARE_VERSION, rebuild, upload .pio/build/esp32dev/firmware.bin
// to ThingsBoard as an OTA package with the same title and version.
#pragma once

// Must equal the "Title" of the ThingsBoard OTA package. The device refuses
// packages with any other title.
#define FIRMWARE_TITLE "BuddyBot"

// Version of THIS build. The device installs whatever package version is
// assigned to it in ThingsBoard if it differs from this (downgrades included),
// unless that version previously failed to boot (see interface-contract.md).
#define FIRMWARE_VERSION "0.3.4"

// 1 = run one update check as soon as the ThingsBoard connection is up after boot.
// A check can always be started manually with the serial command "ota".
#define OTA_CHECK_ON_BOOT 1

// TEST ONLY. 1 = a freshly OTA-installed image deliberately crashes before
// marking itself valid, so you can watch the bootloader roll back to the
// previous firmware. Has no effect on USB-flashed or already-confirmed images.
// Never ship a build with this set.
#define OTA_TEST_CRASH_BEFORE_VALID 0
