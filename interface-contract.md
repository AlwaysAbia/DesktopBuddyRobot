# Interface Contract — Desktop Robot

This is the single source of truth for every interface that crosses a
firmware / mobile app / server boundary. Whenever a Claude Code session
defines or changes a UUID, NVS key, RPC method, or attribute name, **update
this file in the same session**, before finishing. Later sessions (especially
the Flutter app session) are built directly against what's written here.

Keep this file at the repository root (shared by firmware, mobile app and
server) so every session can find it.

---

## 1. BLE GATT Service (Firmware ↔ Mobile App)

**Defined in:** Session D (Phase 2b)

- Service UUID: `TBD`

| Characteristic     | UUID  | Direction        | Format | Notes                              |
|---------------------|-------|-------------------|--------|-------------------------------------|
| WiFi Config          | `TBD` | Write (App→Device)| `TBD`  | SSID + password                     |
| Eye Color             | `TBD` | Write             | `TBD`  |                                      |
| Mode Select           | `TBD` | Write             | `TBD`  | 0 = Eye, 1 = Time, 2 = History      |
| OTA Check Trigger     | `TBD` | Write             | `TBD`  | Any write triggers a check          |
| Status                | `TBD` | Read / Notify     | `TBD`  | WiFi state, current mode, fw version|

Pairing: "just works" (no PIN/bonding).

**Offline requirement (decided 2026-09-29):** the robot must work with no WiFi and no internet. The eye animation runs at boot without waiting for WiFi, and BLE Eye Color and Mode Select must work fully offline (BLE never depends on WiFi or ThingsBoard). There is **no offline messaging**: messages come only from ThingsBoard (Phase 4). BLE should use NimBLE, not Bluedroid (flash: the app is ~1.2 MB of a 1.875 MB OTA slot).

---

## 2. NVS Keys (Firmware persistence)

**Defined in:** Sessions C & D

| Namespace | Key       | Type   | Purpose |
|-----------|-----------|--------|---------|
| `ota`     | `pending` | string | Firmware version an OTA update just started flashing. Written before the download, cleared on the next boot. |
| `ota`     | `bad`     | string | Version that was flashed but didn't boot (rolled back). Checks skip it (and report `FAILED`); `ota force` overrides. Cleared when an update succeeds. |
| `msgs`    | `m0`      | string | Newest stored message (max 100 chars). Missing key = empty slot. |
| `msgs`    | `m1`      | string | Second newest message. |
| `msgs`    | `m2`      | string | Oldest of the 3 stored messages. A new message shifts `m0`→`m1`→`m2` and drops the old `m2`. |

Decided, to be implemented in Session D (not in firmware yet):

| Namespace | Key         | Type | Purpose |
|-----------|-------------|------|---------|
| `display` | `eye_theme` | u8   | Last selected eye color (`eye::Theme` number). Written whenever the color changes, read at boot. Until the first write, the boot default (Amber) applies. |
| `time`    | `last`      | u64  | Last known Unix time (seconds, UTC). Saved after each NTP sync and then periodically. At boot without WiFi the clock starts from it and counts on the ESP32's internal ticks, so it stays roughly right offline. Time while the robot was unpowered is not counted, and internal ticks drift; the next NTP sync corrects both. |

Still to decide (fill in exact keys as sessions define them):
- Last selected mode
- WiFi SSID / password

Display modes (firmware `modes::Mode`, same numbers as BLE Mode Select): `0` = `EYE_ANIMATION`, `1` = `CURRENT_TIME`, `2` = `MESSAGE_HISTORY`. Boot mode is `EYE_ANIMATION` (not persisted yet). Eye color themes (`eye::Theme`): `0` = Cyan, `1` = Amber (boot default), `2` = Emerald, `3` = Magenta.

---

## 3. ThingsBoard Integration (Firmware ↔ Server)

**Defined in:** Sessions B & F

- Server: ThingsBoard Cloud EU, host `eu.thingsboard.cloud` (self-hosted CE planned later: same protocol, just a host/token swap)
- Transport: MQTT over TLS, port `8883`, server cert checked against the ESP32 core's built-in Mozilla root CA bundle. One persistent connection (`BuddyBotFirmware/src/tb_client.cpp`) carries OTA now and messaging later.
- Host/port in firmware: `TB_HOST` / `TB_PORT` in `BuddyBotFirmware/include/thingsboard_config.h`
- Device profile name: `BuddyBot`
- Device name: `buddybot-01`
- Access token: `TB_ACCESS_TOKEN` in `BuddyBotFirmware/include/secrets.h` (gitignored; template in `secrets.example.h`). Never committed.
- Attributes:
  - Firmware version reported as: client attributes `current_fw_title` / `current_fw_version`, sent on every (re)connect. The same two keys are also sent as telemetry, because ThingsBoard's OTA dashboard reads them from there.
  - OTA state reported as telemetry `fw_state` (`DOWNLOADING` → `DOWNLOADED` → `UPDATING` → `UPDATED`, or `FAILED`) plus `fw_error`. `UPDATED` / `FAILED` (for a rollback) is sent after the reboot, on the first connect.
  - OTA-availability push attribute: none of our own. ThingsBoard sets the shared attributes `fw_title`, `fw_version`, `fw_size`, `fw_checksum`, `fw_checksum_algorithm` when a package is assigned. The device reads them only when a check runs (boot or serial `ota`), so it does not auto-update on assignment yet.
- RPC methods:
  - Incoming message method name: `TBD` (parameter format: `TBD`)
  - **TEMPORARY** test RPCs (`BuddyBotFirmware/src/remote_test.cpp`), two-way (`nextMode`, `clearMessages`, `reboot` take no params). They'll be removed when BLE mode control and Phase 4 messaging replace them:
    - `nextMode`: switches to the next display mode.
    - `clearMessages`: deletes the stored message history (RAM + NVS `msgs/*`).
    - `addMessage`: params `{"text": "..."}` (or a plain JSON string); stores it as the newest message (max 100 chars). CR / LF characters are removed; a message that is empty after that is ignored.
    - `reboot`: restarts the device ~1.5 s after responding.
    - All four respond `{"mode": "<EYE_ANIMATION|CURRENT_TIME|MESSAGE_HISTORY>", "timeSynced": <bool>, "messages": <0-3>}` (state after the action).
- OTA package naming convention:
  - Package **Title** = `BuddyBot` (must equal `FIRMWARE_TITLE` in `BuddyBotFirmware/include/ota_config.h`; the device rejects other titles)
  - Package **Version** = `FIRMWARE_VERSION` of the build, `MAJOR.MINOR.PATCH`
  - Package type Firmware, device profile `BuddyBot`, binary = `BuddyBotFirmware/.pio/build/esp32dev/firmware.bin`, checksum MD5 or SHA-256 (auto-generated is fine)
  - Install rule: installs the assigned version if it differs from the running one (downgrades allowed), unless it is the NVS `ota/bad` version
- Serial commands (115200 baud): `ota`, `ota force`, `status`, plus TEMPORARY `mode` (next display mode) and `msg clear` (same as the RPCs above)

---

## Change log

| Session | Date | What changed |
|---------|------|----------------|
| B (Stage 1) | 2026-09-28 | Added NVS keys `ota/pending` and `ota/bad`; added section 4 (pull-OTA manifest format) |
| B (Stage 2) | 2026-09-28 | Filled in section 3 (ThingsBoard host, profile `BuddyBot`, device `buddybot-01`, token location, `current_fw_*` attributes, OTA package naming). Removed section 4 (pull-OTA manifest), replaced by ThingsBoard OTA. NVS `ota/*` keys now used by ThingsBoard OTA. |
| C (Phase 2a) | 2026-09-29 | Added NVS keys `msgs/m0`..`msgs/m2` (message history), display mode and eye theme numbering, TEMPORARY test RPCs `nextMode` / `clearMessages` and serial `mode` / `msg clear`. |
| C (Phase 2a fixes) | 2026-09-29 | TEMPORARY RPCs `addMessage` (params `{"text"}`) and `reboot` added for remote testing. Display: clock and text use an even-pitch LED map, `SCREEN_Y_SIGN` -1, text tilted 45 degrees (`TEXT_ROTATION_DEG`) with a 5x7 font; RPC method/response-field limits raised to 8. |
| C (close-out) | 2026-09-29 | Recorded the offline requirement (eye at boot without WiFi, BLE mode/color offline, no offline messages, NimBLE) and the decided NVS keys `display/eye_theme` and `time/last` (not implemented yet). |
