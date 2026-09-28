# DesktopBuddyRobot

ESP32 desktop robot: FastLED eye animation (76 WS2812B LEDs), WiFi, OTA
updates from ThingsBoard Cloud. Mobile app and BLE come later.

- `BuddyBotFirmware/`: PlatformIO project (arduino-esp32 3.2.0 via pioarduino)
- `BuddyRobotDisplay/`: Altium PCB
- `BuddyRobot3DModel/`: Inventor CAD
- `interface-contract.md`: every firmware / app / server interface

## Git workflow

- Before starting, check `git status` is clean and `main` is in sync with `origin/main`.
- Work on a branch: `firmware/<topic>` (or `docs/<topic>`). Commit there.
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

- WiFi credentials and `TB_ACCESS_TOKEN` live in `BuddyBotFirmware/include/secrets.h`
  (gitignored, template in `secrets.example.h`, loaded via `secrets_loader.h`).
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
  - `ota::confirmRunningFirmware()` runs early in `setup()`.
  - NVS `ota/pending` / `ota/bad` stop a rolled-back version from being retried.
- Update checks are manual (boot check + serial `ota` / `ota force` / `status`).
  Future triggers (BLE) should call `ota::requestCheck()`.

## Code conventions

- Serial output: end lines with `\r\n` (`Serial.println` or `"...\r\n"` in printf).
  A bare `\n` staircases in the user's terminal.
- Match the existing style: `[TAG]` log prefixes (`[WiFi]`, `[OTA]`, `[TB]`),
  section banners in `main.cpp`, small modules with a namespace (`ota::`, `tb_client::`).

## Hardware and testing

- Claude has no hardware access. End every firmware change with a numbered manual
  test plan for the user, and say plainly what couldn't be verified.
- The buzzer never sounded during bring-up; its pin is unconfirmed and the self-test
  is disabled (`BUZZER_SELFTEST 0`). Don't build features that depend on it
  until the user confirms the hardware.
