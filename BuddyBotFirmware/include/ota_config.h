// Firmware version + OTA settings. This is the file you touch to cut a new
// OTA build: bump FIRMWARE_VERSION, rebuild, publish the .bin + manifest.
#pragma once

// Semantic version "MAJOR.MINOR.PATCH" of THIS build. The device only
// installs a manifest version that compares strictly greater than this.
#define FIRMWARE_VERSION "0.1.0"

// Where the device looks for the update manifest. http:// (local test server)
// and https:// (GitHub raw, etc. - verified against the built-in CA bundle)
// both work. Manifest format: see interface-contract.md, section 4.
#define OTA_MANIFEST_URL "http://192.168.1.100:8000/manifest.json"

// 1 = run one update check right after WiFi connects on boot.
// A check can always be started manually with the serial command "ota".
#define OTA_CHECK_ON_BOOT 1

// TEST ONLY. 1 = a freshly OTA-installed image deliberately crashes before
// marking itself valid, so you can watch the bootloader roll back to the
// previous firmware. Has no effect on USB-flashed or already-confirmed images.
// Never ship a build with this set.
#define OTA_TEST_CRASH_BEFORE_VALID 0
