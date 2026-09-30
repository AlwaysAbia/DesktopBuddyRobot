# Remote test plan (ThingsBoard dashboard only)

For firmware 0.6.0 and later. No serial, no USB. Device: `buddybot-01`. RPCs are sent from a
dashboard RPC widget or the device's "RPC" tab (two-way). Keys are in
[../interface-contract.md](../interface-contract.md), section 3.

## A. Status reporting

1. After the robot connects, check **Attributes → Client**: `ota_partition`, `ota_image_state`,
   `ota_rollback_possible`, `ota_awaiting_confirm`, `ota_bad_version`, `ota_last_error`,
   `current_fw_version`. Expect `ota_image_state` = `VALID` (or `n/a` after a USB flash), `ota_awaiting_confirm` = false.
2. **Latest telemetry** shows `rssi`, `uptime_s`, `heap_free_kb`, `heap_min_kb`, refreshed about every 60 s.
   `uptime_s` should rise by about 60 between refreshes.
3. Send RPC `reboot` (temporary RPC). Within about a minute `uptime_s` restarts near 0 and the attributes reappear.

## B. Remote update trigger

Prerequisite: a package with a higher version than the running one is assigned to the device.

4. Send RPC `checkForUpdate` with params `{}`. Response: `{"accepted": true, "force": false}`.
5. Watch `fw_state`: `DOWNLOADING` → `DOWNLOADED` → `UPDATING`. The robot reboots, reconnects, and
   `current_fw_version` shows the new version, `fw_state` = `UPDATED`.
6. Send `checkForUpdate` twice quickly. The second response should be `accepted: false`.
7. With the same version already assigned, `checkForUpdate` leaves `fw_state` = `UPDATED` and nothing reinstalls.
8. Params `{"force": true}` retries a version listed in `ota_bad_version`.

## C. Confirm only after connecting

9. Right after step 5's reboot, `ota_awaiting_confirm` may briefly be true and `ota_image_state`
   `PENDING_VERIFY`; within seconds of the first connect they change to false / `VALID`.
10. **Timeout rollback** (needs a special build): set `OTA_TEST_SKIP_CONFIRM 1`, bump the version above
    the one on the robot, build, upload and assign it (do not commit this change). After the update,
    `ota_awaiting_confirm` stays true for about 5 minutes although the robot is online, then the robot
    reboots into the old version. Expect: `current_fw_version` back to the old one, `fw_state` = `FAILED`,
    `ota_bad_version` = the test version, `ota_last_error` mentions the rollback. `checkForUpdate` without
    `force` must not reinstall it.
11. Then upload a normal build with a higher version and check it installs and reaches `VALID`.

## D. Safety notes

- The very first install of 0.6.0 goes through 0.5.2's OTA path, which is unchanged. Keep USB access
  available for that one update in case something unexpected happens.
- A broken WiFi or ThingsBoard connection in a new build now causes a rollback after 5 minutes;
  a router that is off for more than 5 minutes right after an update also causes one (the version is
  then flagged bad and needs a higher version number).
