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
| `ota`     | `pending` | string | Version the pull OTA just flashed. Written before download, cleared on the next boot. |
| `ota`     | `bad`     | string | Version that was flashed but didn't boot (rolled back). Auto-checks skip it; `ota force` overrides. Cleared when a pull update succeeds. |

Known items to persist (fill in exact keys as sessions define them):
- Last selected mode
- Last selected eye color
- WiFi SSID / password
- Last 3 received messages

---

## 3. ThingsBoard Integration (Firmware ↔ Server)

**Defined in:** Sessions B & F

- Server: ThingsBoard Cloud (self-hosted CE planned later — same protocol, just a host/token swap)
- Device profile name: `TBD`
- Access token: stored where — `TBD`
- Attributes:
  - Firmware version reported as: `TBD`
  - OTA-availability push attribute: `TBD`
- RPC methods:
  - Incoming message method name: `TBD` (parameter format: `TBD`)
- OTA package naming convention: `TBD`

---

## 4. HTTP(S) Pull OTA Manifest (Firmware ↔ self-hosted file server)

**Defined in:** Session B, Stage 1. Interim transport, replaced by ThingsBoard OTA in Stage 2.

- Manifest URL: `OTA_MANIFEST_URL` in `BuddyBotFirmware/include/ota_config.h` (http:// or https://)
- Running version: `FIRMWARE_VERSION` in the same file, `MAJOR.MINOR.PATCH` (an optional leading `v` is accepted)
- HTTPS is verified against the ESP32 core's built-in Mozilla root CA bundle. Redirects are followed.
- Manifest body (JSON):

```json
{
  "version": "0.1.1",
  "url": "https://host/path/firmware-0.1.1.bin",
  "md5": "32 lowercase hex chars (optional, checked after the download if present)"
}
```

- Install rule: install only if `version` is strictly greater than `FIRMWARE_VERSION` and is not the NVS `ota/bad` version.
- File naming: `firmware-<version>.bin`, produced by `BuddyBotFirmware/tools/make_ota_manifest.py`
- Serial commands (115200 baud): `ota`, `ota force`, `status`

---

## Change log

| Session | Date | What changed |
|---------|------|----------------|
| B (Stage 1) | 2026-09-28 | Added NVS keys `ota/pending` and `ota/bad`; added section 4 (pull-OTA manifest format) |
