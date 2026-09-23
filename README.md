# Desktop Buddy Robot

A desktop robot with a circular addressable-LED "eye" driven by an ESP32.

## Repository layout

| Folder | Contents | Tool |
|---|---|---|
| `BuddyBotFirmware/` | ESP32 firmware (eye animation, WiFi, OTA) | PlatformIO |
| `BuddyRobotDisplay/` | LED display PCB (schematics, layout, libraries, OutJob) | Altium Designer |
| `BuddyRobot3DModel/` | Enclosure CAD and printable STLs | Autodesk Inventor |

Interfaces shared between firmware, the mobile app and the server are documented in
[`BuddyBotFirmware/interface-contract.md`](BuddyBotFirmware/interface-contract.md).

## Firmware quick start

1. Install [PlatformIO](https://platformio.org/) (VS Code extension or CLI).
2. Copy `BuddyBotFirmware/include/secrets.example.h` to `BuddyBotFirmware/include/secrets.h` and fill in your WiFi credentials.
3. From `BuddyBotFirmware/`:
   - Build: `pio run`
   - Flash over USB: `pio run -t upload`
   - Flash over WiFi (ArduinoOTA): `pio run -e esp32dev_ota -t upload`
   - Serial monitor: `pio device monitor`

## License

MIT, see [LICENSE](LICENSE).
