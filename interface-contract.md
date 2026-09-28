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

---

## 2. NVS Keys (Firmware persistence)

**Defined in:** Sessions C & D

| Namespace | Key       | Type   | Purpose |
|-----------|-----------|--------|---------|
| `ota`     | `pending` | string | Firmware version an OTA update just started flashing. Written before the download, cleared on the next boot. |
| `ota`     | `bad`     | string | Version that was flashed but didn't boot (rolled back). Checks skip it (and report `FAILED`); `ota force` overrides. Cleared when an update succeeds. |

Known items to persist (fill in exact keys as sessions define them):
- Last selected mode
- Last selected eye color
- WiFi SSID / password
- Last 3 received messages

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
- OTA package naming convention:
  - Package **Title** = `BuddyBot` (must equal `FIRMWARE_TITLE` in `BuddyBotFirmware/include/ota_config.h`; the device rejects other titles)
  - Package **Version** = `FIRMWARE_VERSION` of the build, `MAJOR.MINOR.PATCH`
  - Package type Firmware, device profile `BuddyBot`, binary = `BuddyBotFirmware/.pio/build/esp32dev/firmware.bin`, checksum MD5 or SHA-256 (auto-generated is fine)
  - Install rule: installs the assigned version if it differs from the running one (downgrades allowed), unless it is the NVS `ota/bad` version
- Serial commands (115200 baud): `ota`, `ota force`, `status`

---

## Change log

| Session | Date | What changed |
|---------|------|----------------|
| B (Stage 1) | 2026-09-28 | Added NVS keys `ota/pending` and `ota/bad`; added section 4 (pull-OTA manifest format) |
| B (Stage 2) | 2026-09-28 | Filled in section 3 (ThingsBoard host, profile `BuddyBot`, device `buddybot-01`, token location, `current_fw_*` attributes, OTA package naming). Removed section 4 (pull-OTA manifest), replaced by ThingsBoard OTA. NVS `ota/*` keys now used by ThingsBoard OTA. |
