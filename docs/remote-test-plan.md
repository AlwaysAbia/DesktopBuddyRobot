# Remote test plan (ThingsBoard dashboard only)

For firmware 0.6.0 and later. No serial, no USB. Device: `buddybot-01`. RPCs are sent from a
dashboard RPC widget or the device's "RPC" tab (two-way). Keys are in
[../interface-contract.md](../interface-contract.md), section 3.

Done and confirmed on the robot (2026-09-30):
- Status attributes and telemetry (0.6.0), the `checkForUpdate` RPC (with and without `force`).
- Mode, eye color and message attributes (0.6.1), including after `reboot`.
- Timeout rollback: a build with `OTA_TEST_SKIP_CONFIRM 1` stayed unconfirmed, rolled back to the
  previous version after about 5 minutes, was flagged as `ota_bad_version`, and was not downloaded
  again by a plain `checkForUpdate`.

## Repeat this test for any change to the OTA or rollback code

1. Set `OTA_TEST_SKIP_CONFIRM 1` and a version above the running one in `ota_config.h` (do not commit it).
   Build, upload, assign.
2. `checkForUpdate` with `{}`. After the reboot: `ota_awaiting_confirm` true, `ota_image_state`
   `PENDING_VERIFY`, and it stays that way although the robot is online. Do not reboot it by hand
   (a reset rolls back at once through the bootloader, which is a different path).
3. After about 5 minutes (`uptime_s` near 300) the robot restarts on the previous version:
   `fw_state` `FAILED`, `ota_bad_version` = the test version, `ota_last_error` mentions the rollback.
4. `checkForUpdate` with `{}` must not reinstall it. `{"force": true}` does.
5. Restore `ota_config.h` (`git checkout`) and confirm a normal build reaches `VALID`.

## Safety notes

- A broken WiFi or ThingsBoard connection in a new build causes a rollback after 5 minutes;
  a router that is off for more than 5 minutes right after an update also causes one (the version is
  then flagged bad and needs a higher version number).
- A download that is interrupted (reboot or a second check while it runs) also flags that version bad.
  `checkForUpdate` with `{"force": true}` retries it.
