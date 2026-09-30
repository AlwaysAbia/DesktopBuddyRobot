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

**Defined in:** Session D (Phase 2b). Implemented in firmware 0.5.0 (`BuddyBotFirmware/src/ble_control.cpp`).

- Advertised device name: `BuddyBot` (in the scan response). The service UUID is in the advertising packet, so scan by UUID.
- Role: the robot is the GATT peripheral/server; the app is the central. One client at a time; advertising resumes after a disconnect.
- Service UUID: `9b370000-a32f-4baf-8406-88e9298fd20d`

| Characteristic    | UUID                                   | Properties                      | Payload |
|-------------------|----------------------------------------|---------------------------------|---------|
| WiFi Config       | `9b370001-a32f-4baf-8406-88e9298fd20d` | Write (encrypted link required) | UTF-8 JSON, see below |
| Eye Color         | `9b370002-a32f-4baf-8406-88e9298fd20d` | Read, Write                     | 1 byte, `0`–`3` |
| Mode Select       | `9b370003-a32f-4baf-8406-88e9298fd20d` | Read, Write                     | 1 byte, `0`–`2` |
| OTA Check Trigger | `9b370004-a32f-4baf-8406-88e9298fd20d` | Write                           | any payload (even 0 bytes), value ignored |
| Status            | `9b370005-a32f-4baf-8406-88e9298fd20d` | Read, Notify                    | UTF-8 JSON, see below |

Use Write (with response) only; do not use Write Without Response. Each characteristic also has a read-only Characteristic User Description (0x2901) with its name from the first column (added in 0.5.1), so generic tools can label them.

**Pairing / security:** "Just Works" (LE Secure Connections, IO capability none): no PIN, no passkey, **no bonding** (keys are not stored on the robot, so the phone may pair again on each connection). Only WiFi Config demands an encrypted link, so the phone shows its pairing prompt the first time the app writes it; everything else works on an unencrypted link. Just Works protects against passive sniffing, not against a man in the middle. Anyone within radio range can change the eye color or mode and start an OTA check; only WiFi Config needs the encrypted link. Compile-time switch: `BLE_WIFI_CONFIG_REQUIRES_ENCRYPTION` in `ble_config.h`.

**MTU:** the app must negotiate an MTU of at least 128 (Android: `requestMtu(247)` right after connecting; iOS does it automatically) before writing WiFi Config or enabling Status notifications. The robot accepts up to 255. With the default MTU of 23 a Status notification that does not fit is **not sent** (reads are always complete, so the app can fall back to reading).

**Invalid writes are ignored silently.** The BLE stack cannot return an ATT error from the write handler; the reason is logged on the robot's serial port. The app must validate before writing and confirm the effect by reading the value back or watching Status.

### WiFi Config (write)

UTF-8 JSON object, at most 200 bytes:

```json
{"ssid": "MyNetwork", "password": "secret123"}
```

| Field      | Type   | Rules |
|------------|--------|-------|
| `ssid`     | string | Required. 1–32 bytes (UTF-8). An empty string `""` means "forget the stored network": the robot disconnects and stays offline until new credentials arrive. |
| `password` | string | Optional (missing = `""`). Empty for an open network, otherwise 8–63 characters, or exactly 64 hex characters. |

On a valid write the robot stores the credentials in NVS (`wifi/ssid`, `wifi/pass`) and starts connecting in the background, dropping any current WiFi connection at once. It does not reboot. Progress shows in Status `wifi`: `connecting`, then `connected`, or `failed` (with `wifiErr`) after 20 s. While failed, the robot retries every 30 s. New credentials replace the stored ones immediately (there is no "test first" step), so if the app wants to be careful it must re-send the old credentials when it sees `failed`. The password can never be read back.

### Eye Color (read / write)

1 byte, the `eye::Theme` number: `0` = Cyan, `1` = Amber, `2` = Emerald, `3` = Magenta. Applied on the next frame (visible in EYE_ANIMATION) and saved in NVS (`display/eye_theme`). A read returns the current color. Values above `3`, or payloads that are not exactly 1 byte, are ignored.

### Mode Select (read / write)

1 byte: `0` = EYE_ANIMATION, `1` = CURRENT_TIME, `2` = MESSAGE_HISTORY. Applied at once, saved in NVS (`display/mode`) and restored at boot. A read returns the current mode. Values above `2`, or payloads that are not exactly 1 byte, are ignored.

### OTA Check Trigger (write)

Any write starts the same check as the serial command `ota`: the robot asks ThingsBoard which firmware is assigned and installs it if the version differs from the running one. If ThingsBoard is not connected yet the check waits until it is; without WiFi nothing happens. A check already in progress is not restarted. Progress is reported to ThingsBoard (`fw_state`), not over BLE; Status only says whether ThingsBoard is connected (`tb`).

### Status (read / notify)

UTF-8 JSON object, about 100 bytes:

```json
{"wifi":"connected","mode":0,"eye":1,"tb":true,"fw":"0.5.0","heapMinKb":58}
```

| Field       | Type   | Meaning |
|-------------|--------|---------|
| `wifi`      | string | `none` (no credentials stored), `connecting`, `connected`, `failed` |
| `wifiErr`   | string | Only present while `wifi` is `failed`: `not_found` (network not seen), `auth` (wrong password or handshake failed), `other` |
| `mode`      | number | Current mode, as in Mode Select |
| `eye`       | number | Current eye color, as in Eye Color |
| `tb`        | bool   | ThingsBoard MQTT connection is up |
| `fw`        | string | Running firmware version (`FIRMWARE_VERSION`) |
| `heapMinKb` | number | Lowest free heap since boot, in KB (diagnostic) |

Notified (if the app subscribed) whenever `wifi`, `mode`, `eye` or `tb` changes, and after **every** write to any writable characteristic above (valid or not), so the app can also use a write plus its notification as a round-trip probe. Reading Status always returns the current state. New fields may be added later, so the app must ignore fields it does not know.

**Offline requirement (decided 2026-09-29):** the robot must work with no WiFi and no internet. The eye animation runs at boot without waiting for WiFi, and BLE Eye Color and Mode Select work fully offline (BLE never depends on WiFi or ThingsBoard). There is **no offline messaging**: messages come only from ThingsBoard (Phase 4). BLE uses NimBLE (NimBLE-Arduino 2.3.7), not Bluedroid.

**Shared radio:** the ESP32 has one radio for WiFi and BLE. While a BLE client is connected the robot puts WiFi into modem sleep (`BLE_WIFI_SLEEP_WHILE_CONNECTED` in `ble_config.h`) so BLE gets air time, and back to always-awake when the client disconnects. Expect a slightly slower ThingsBoard connection during a BLE session. Coexistence was **not verified on hardware** in Session D (no hardware access): see its test plan.

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

Added in Session D (firmware 0.5.0):

| Namespace | Key         | Type   | Purpose |
|-----------|-------------|--------|---------|
| `wifi`    | `ssid`      | string | Stored WiFi network name, empty = forgotten. **Missing key = genuinely first-ever boot**: the firmware then seeds `ssid` / `pass` from `WIFI_SSID` / `WIFI_PASSWORD` in `secrets.h` (may be empty) and never reads `secrets.h` again. Erased NVS therefore falls back to those build-time values once. |
| `wifi`    | `pass`      | string | Stored WiFi password (plain text; NVS encryption is not enabled). |
| `display` | `eye_theme` | u8     | Last selected eye color (`eye::Theme` number). Written when it changes, read at boot. Missing key = Amber (boot default). |
| `display` | `mode`      | u8     | Last selected display mode (`modes::Mode` number). Written when it changes (BLE, RPC `nextMode`), restored at boot. Missing key = `0` (eye). |
| `time`    | `last`      | u64    | Last known Unix time (seconds, UTC). Saved at the first NTP sync after boot and every 30 minutes after that. At boot without WiFi the clock starts from it and counts on the ESP32's internal ticks, so it stays roughly right offline. Time while the robot was unpowered is not counted, and internal ticks drift; the next NTP sync corrects both. |

Display modes (firmware `modes::Mode`, same numbers as BLE Mode Select): `0` = `EYE_ANIMATION`, `1` = `CURRENT_TIME`, `2` = `MESSAGE_HISTORY`. Boot mode is the persisted one (`display/mode`), `EYE_ANIMATION` on the first boot. Eye color themes (`eye::Theme`): `0` = Cyan, `1` = Amber (boot default), `2` = Emerald, `3` = Magenta.

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
- Device status (firmware 0.6.0, `BuddyBotFirmware/src/remote_ops.cpp`), sent on every (re)connect:
  - Client attributes, resent whenever they change (checked every 5 s): `ota_partition` (running slot, `app0`/`app1`), `ota_image_state` (`NEW`, `PENDING_VERIFY`, `VALID`, `INVALID`, `ABORTED`, `UNDEFINED`, `n/a`), `ota_rollback_possible` (bool), `ota_awaiting_confirm` (bool, true while a new image has not yet connected to ThingsBoard), `ota_bad_version` (`-` = none), `ota_last_error` (`-` = none; cleared by a successful update).
  - Also client attributes (0.6.1): `mode` (`EYE_ANIMATION`, `CURRENT_TIME`, `MESSAGE_HISTORY`), `eye_color` (`Cyan`, `Amber`, `Emerald`, `Magenta`), `msg_count` (0-3), `msg_latest` (newest stored message, `-` = none).
  - Telemetry, every 60 s: `rssi` (dBm), `uptime_s`, `heap_free_kb`, `heap_min_kb`.
- Rollback confirmation (0.6.0): a freshly OTA-installed image is **not** marked valid at boot. It is confirmed at the first successful ThingsBoard connection (`ota_image_state` goes `PENDING_VERIFY` → `VALID`). If that does not happen within `OTA_CONFIRM_TIMEOUT_S` (300 s) of boot, or the robot resets first, the bootloader rolls back to the previous firmware and the failed version is flagged `ota/bad`.
- RPC methods:
  - `checkForUpdate` (0.6.0, permanent): params `{"force": true}` (optional; a plain `true` also works). Same as serial `ota` / `ota force`. Responds `{"accepted": <bool>, "force": <bool>}`; `accepted: false` means a check or update was already running. Progress is the `fw_state` telemetry.
  - Kept on purpose (decided 2026-09-30): these RPCs (`BuddyBotFirmware/src/remote_test.cpp`, still named "test" in the code) are the way messages reach the robot, so there is no separate messaging method. Two-way (`nextMode`, `clearMessages`, `reboot` take no params):
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
- Serial commands (115200 baud): `ota`, `ota force`, `status`, plus `msg clear` (same as the RPC above). The serial `mode` command was removed in 0.5.0: the app switches modes over BLE. The RPC `nextMode` stays

---

## Change log

| Session | Date | What changed |
|---------|------|----------------|
| B (Stage 1) | 2026-09-28 | Added NVS keys `ota/pending` and `ota/bad`; added section 4 (pull-OTA manifest format) |
| B (Stage 2) | 2026-09-28 | Filled in section 3 (ThingsBoard host, profile `BuddyBot`, device `buddybot-01`, token location, `current_fw_*` attributes, OTA package naming). Removed section 4 (pull-OTA manifest), replaced by ThingsBoard OTA. NVS `ota/*` keys now used by ThingsBoard OTA. |
| C (Phase 2a) | 2026-09-29 | Added NVS keys `msgs/m0`..`msgs/m2` (message history), display mode and eye theme numbering, TEMPORARY test RPCs `nextMode` / `clearMessages` and serial `mode` / `msg clear`. |
| C (Phase 2a fixes) | 2026-09-29 | TEMPORARY RPCs `addMessage` (params `{"text"}`) and `reboot` added for remote testing. Display: clock and text use an even-pitch LED map, `SCREEN_Y_SIGN` -1, text tilted 45 degrees (`TEXT_ROTATION_DEG`) with a 5x7 font; RPC method/response-field limits raised to 8. |
| C (close-out) | 2026-09-29 | Recorded the offline requirement (eye at boot without WiFi, BLE mode/color offline, no offline messages, NimBLE) and the decided NVS keys `display/eye_theme` and `time/last` (not implemented yet). |
| D (Phase 2b) | 2026-09-29 | Defined the BLE GATT service (section 1: service and 5 characteristic UUIDs, payload formats, security, MTU, Status JSON). NVS keys `wifi/ssid`, `wifi/pass`, `display/eye_theme`, `display/mode`, `time/last` implemented. WiFi now connects in the background from NVS credentials (seeded from `secrets.h` on the first-ever boot only). Serial `mode` command removed. |
| D (0.5.1) | 2026-09-29 | Added a Characteristic User Description (0x2901) to each BLE characteristic. No UUID or payload change. |
| Remote operation | 2026-09-30 | Firmware 0.6.0: RPC `checkForUpdate`, client attributes `ota_*` and telemetry `rssi` / `uptime_s` / `heap_*` (section 3), and rollback confirmation moved to the first ThingsBoard connection with a 300 s timeout. No BLE change. |
| Remote operation (0.6.1) | 2026-09-30 | Client attributes `mode`, `eye_color`, `msg_count`, `msg_latest` (section 3). |
| E (Flutter app) | 2026-09-30 | New app `BuddyBotApp/` implements section 1 as written; no contract change. App-side behavior: scans by service UUID, requests MTU 247, subscribes to Status, treats a WiFi result as valid only after seeing `connecting` (or 6 s grace), retries a WiFi Config write once on a BLE error (pairing prompt), auto-reconnects with backoff (1, 2, 3, 5, 8, 10 s) after a drop. |
