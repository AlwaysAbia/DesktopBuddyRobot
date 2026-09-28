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

| Key   | Type | Purpose |
|-------|------|---------|
| `TBD` |      |         |

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

## Change log

| Session | Date | What changed |
|---------|------|----------------|
|         |      |                |
