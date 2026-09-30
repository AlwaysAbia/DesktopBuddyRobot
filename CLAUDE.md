# DesktopBuddyRobot

ESP32 desktop robot: FastLED eye animation (76 WS2812B LEDs), WiFi, OTA
updates from ThingsBoard Cloud, BLE control (NimBLE), Flutter companion app.

- `BuddyBotFirmware/`: PlatformIO project (arduino-esp32 3.2.0 via pioarduino)
- `BuddyBotApp/`: Flutter app (Android + iOS), BLE only. Setup in its README
- `BuddyRobotDisplay/`: Altium PCB
- `BuddyRobot3DModel/`: Inventor CAD
- `interface-contract.md`: every firmware / app / server interface

## Git workflow

- Before starting, check `git status` is clean and `main` is in sync with `origin/main`.
- Work on a branch: `firmware/<topic>`, `app/<topic>` or `docs/<topic>`. Commit there.
- When the user says so: merge into `main` with `--no-ff`, push, delete the branch.
- Don't commit local test changes (e.g. a temporary `FIRMWARE_VERSION` bump or `OTA_TEST_CRASH_BEFORE_VALID 1`). Stash them around merges if needed.

## Interface contract

- `interface-contract.md` at the repo root is the single source of truth.
  `BuddyBotFirmware/INTERFACE-CONTRACT.md` only points to it. Never keep a second copy.
- Whenever a session adds or changes a BLE UUID, NVS key, ThingsBoard name,
  attribute, RPC method or OTA naming, update the contract (and its change log)
  in the same session.

## Building

- `pio` is not on PATH. Use `~/.platformio/penv/Scripts/pio.exe`.
  - Build: `~/.platformio/penv/Scripts/pio.exe run -e esp32dev` (from `BuddyBotFirmware/`)
  - USB upload (first flash / recovery only): add `-t upload`
- Edit `lib_deps` in `platformio.ini` by hand. `pio pkg install/uninstall`
  rewrites the file and strips its comments.
- ThingsBoard SDK 0.15.0 requires ArduinoJson 6, so don't add ArduinoJson 7.
- Partition table is `min_spiffs.csv` (two 1.875 MB OTA slots). Changing it needs a USB flash.

## Secrets

- `TB_ACCESS_TOKEN` and the first-boot WiFi credentials live in `BuddyBotFirmware/include/secrets.h`
  (gitignored, template in `secrets.example.h`, loaded via `secrets_loader.h`). WiFi credentials
  are only a seed: `wifi_manager` copies them to NVS on the first-ever boot; after that the app sets
  them over BLE. Keep them filled in for OTA builds so an already-running robot keeps its network.
- Never commit secrets, print them, or ask the user to paste the token into chat.
  To check it's set, test for the define without printing its value.

## OTA / releases

- ThingsBoard OTA is the only over-the-air path. ArduinoOTA was removed on purpose.
- Release: bump `FIRMWARE_VERSION` in `include/ota_config.h`, build, upload
  `.pio/build/esp32dev/firmware.bin` as a ThingsBoard OTA package titled `BuddyBot`
  (must equal `FIRMWARE_TITLE`), assign it to device `buddybot-01`.
  Always use a version higher than what's on the device.
- Rollback safety must be kept in every OTA change:
  - `verifyRollbackLater()` is overridden to return true.
  - `ota::confirmRunningFirmware()` runs early in `setup()`; it defers marking a new image valid until the
    first ThingsBoard connect (`onTbConnected`), and `ota::loop()` rolls back after `OTA_CONFIRM_TIMEOUT_S`.
  - NVS `ota/pending` / `ota/bad` stop a rolled-back version from being retried.
  - An interrupted download (reboot, RPC, power loss) also flags that version `bad`. The only fix without
    serial is a new version string, so re-upload the same build with a higher `FIRMWARE_VERSION`.
- The robot has a BLE GATT server (`ble_control`) and background WiFi from NVS (`wifi_manager`). BLE
  layout and how to test with nRF Connect (request MTU 247): `interface-contract.md`, `docs/ble-basics.md`.
- Update checks are manual: boot check, serial `ota` / `ota force` / `status`, the BLE OTA-check
  characteristic and the ThingsBoard RPC `checkForUpdate` all call `ota::requestCheck()`.

## Code conventions

- Serial output: end lines with `\r\n` (`Serial.println` or `"...\r\n"` in printf).
  A bare `\n` staircases in the user's terminal.
- Match the existing style: `[TAG]` log prefixes (`[WiFi]`, `[OTA]`, `[TB]`),
  section banners in `main.cpp`, small modules with a namespace (`ota::`, `tb_client::`).

- Display code: the eye uses `led_matrix::ledCoords` (rows stretched to full width; leave it, the eye looks right). The clock and text use `led_matrix::screenPos()` (true even-pitch disc, orientation in `include/display_config.h`). Text is tilted 45 degrees on purpose; strokes only come out crisp on the LED grid that way.
- RPC limits live in `tb_client.h` (`MAX_RPC_METHODS`, `MAX_RPC_FIELDS`). Exceeding them makes every RPC time out with no error, so keep the `static_assert` in `remote_test.cpp` in step with them.
- After every scripted edit, `grep` that the change landed (a silently unmatched `replace` once left the text tilt unapplied for four releases). Backslash escapes (the CR/LF in a printf string) get turned into real line breaks by scripted edits, so make those changes with the Edit tool.

## Hardware and testing

- Claude has no hardware access. End every firmware change with a numbered manual
  test plan for the user, and say plainly what couldn't be verified.
- The user works **remotely**: no serial monitor and no USB access to the robot.
  - Write test plans against the ThingsBoard dashboard (attributes, telemetry,
    OTA state), not serial output.
  - Treat every OTA release as unrecoverable if it breaks WiFi or the ThingsBoard
    connection. Rollback is the only safety net, so keep it intact and don't ship
    anything that weakens it.

## Remote operation

- Everything is testable from the ThingsBoard dashboard: RPC `checkForUpdate` (optional `force`), client
  attributes `ota_*`, telemetry `rssi` / `uptime_s` / `heap_*` / `fw_state` (keys in `interface-contract.md`).
  Write test plans against those, never serial.
- A new OTA image is confirmed only after its first ThingsBoard connection (`OTA_CONFIRM_TIMEOUT_S`, then
  rollback). Keep that intact in every OTA change.
- The buzzer never sounded during bring-up; its pin is unconfirmed and the self-test
  is disabled (`BUZZER_SELFTEST 0`). Don't build features that depend on it
  until the user confirms the hardware.

## Continue here: Flutter app (Session E)

State when the last session ended (2026-09-30):
- Branch `app/flutter` holds `BuddyBotApp/` (scan, WiFi setup, control, auto-reconnect, written against
  `interface-contract.md` section 1). `android/` and `ios/` are generated and committed with the BLE
  permissions. `flutter analyze` is clean, `flutter test` passes (11 contract tests) and
  `flutter build apk --debug` succeeds. **It has never been run on a phone.**
- Toolchain: Flutter at `C:\Users\User\MyPrograms\Flutter\flutter\bin` (on the user's PATH but not in
  Claude's shell: `export PATH="$PATH:/c/Users/User/MyPrograms/Flutter/flutter/bin"`). Android SDK in
  `%LOCALAPPDATA%\Android\Sdk`. `flutter doctor` still warns that cmdline-tools are missing (harmless for builds).
  iOS cannot be built on this Windows machine.
- `app/flutter` is pushed to `origin` and deliberately **not merged into `main`**: the user tests at home first.
  Session F starts from the user's test results (fix what broke on `app/flutter`), then merges only when told to.
- Next: the user installs the debug APK on a real Android phone and runs the 12-step manual test plan
  (check `mode`, `eye_color`, `msg_*`, `wifi` on the ThingsBoard dashboard), then merge `app/flutter` into
  `main` when the user says so.
- Least certain in the app (verify on hardware first): WiFi result detection (relies on seeing Status
  `connecting`), the pairing prompt on the first WiFi Config write (retried once), reconnect and
  resubscribe after a drop, MTU handling.
- `firmware/version-0.6.3` (a version bump to match the robot) is committed but not merged into `main`;
  merge it when the user says so. The robot runs 0.6.3; 0.6.2 is flagged bad in its NVS (harmless).
- Messages reach the robot only through the ThingsBoard RPCs `addMessage` etc. (kept on purpose); the app
  never sends messages.
