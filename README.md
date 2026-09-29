# Desktop Buddy Robot

A desktop robot with a circular addressable-LED "eye" driven by an ESP32.

The 76-LED display has three modes: the eye animation (default), an analog clock (time from NTP) and a scrolling history of the last 3 messages. The text is tilted 45 degrees so it lines up with the LED grid; rotate the robot about 45 degrees clockwise to read it. Until Bluetooth control exists, modes are switched with the temporary ThingsBoard RPCs listed in [`interface-contract.md`](interface-contract.md).

## Repository layout

| Folder | Contents | Tool |
|---|---|---|
| `BuddyBotFirmware/` | ESP32 firmware (eye animation, WiFi, OTA) | PlatformIO |
| `BuddyRobotDisplay/` | LED display PCB (schematics, layout, libraries, OutJob) | Altium Designer |
| `BuddyRobot3DModel/` | Enclosure CAD and printable STLs | Autodesk Inventor |

Interfaces shared between firmware, the mobile app and the server are documented in
[`interface-contract.md`](interface-contract.md).

## Firmware quick start

1. Install [PlatformIO](https://platformio.org/) (VS Code extension or CLI).
2. Copy `BuddyBotFirmware/include/secrets.example.h` to `BuddyBotFirmware/include/secrets.h` and fill in your WiFi credentials and ThingsBoard access token.
3. From `BuddyBotFirmware/`:
   - Build: `pio run`
   - Flash over USB (first flash / recovery): `pio run -t upload`
   - Over-the-air updates: upload `.pio/build/esp32dev/firmware.bin` as a ThingsBoard OTA package (see `interface-contract.md`, section 3)
   - Serial monitor: `pio device monitor`

## License

MIT, see [LICENSE](LICENSE).
