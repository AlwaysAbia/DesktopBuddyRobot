// BLE GATT server for the companion app: WiFi provisioning, eye color, display
// mode, OTA-check trigger and a status characteristic. Layout, UUIDs and payload
// formats: interface-contract.md, section 1.
//
// BLE never depends on WiFi or ThingsBoard: it works fully offline.
#pragma once

namespace ble_control {

// Starts NimBLE, the GATT service and advertising. Call once in setup(), after
// eye::begin() / modes::begin() / wifi_mgr::begin().
void begin();

// Applies what the app wrote (the GATT callbacks only queue it, so everything
// runs on the main task) and pushes Status notifications. Call every loop,
// including while an OTA download is running.
void loop();

}  // namespace ble_control
