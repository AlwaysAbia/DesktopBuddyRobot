// BLE settings. The GATT layout itself is defined in interface-contract.md, section 1.
#pragma once

#define BLE_DEVICE_NAME "BuddyBot"

// 1 = the WiFi Config characteristic needs an encrypted link. The phone then runs
// "Just Works" pairing (no PIN, no bond stored) before the first write, so the
// WiFi password is not sent in the clear. Set 0 if pairing causes trouble with a
// client; the password is then readable by anyone sniffing the radio.
#define BLE_WIFI_CONFIG_REQUIRES_ENCRYPTION 1

// 1 = while a BLE client is connected, WiFi switches from "always awake" to modem
// sleep. ESP32 shares one radio between WiFi and BLE, and a WiFi that never sleeps
// can starve BLE of air time. 0 = never touch WiFi power save.
#define BLE_WIFI_SLEEP_WHILE_CONNECTED 1
