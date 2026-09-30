# Remote test plan (ThingsBoard dashboard only)

For firmware 0.6.0 and later. No serial, no USB. Device: `buddybot-01`. RPCs are sent from a
dashboard RPC widget or the device's "RPC" tab (two-way). Keys are in
[../interface-contract.md](../interface-contract.md), section 3.

Done and confirmed on the robot (2026-09-30): status attributes and telemetry (0.6.0), the
`checkForUpdate` RPC, and the update itself.

## Still to test

### 1. Mode, eye color and messages as attributes (0.6.1)

Prerequisite: 0.6.1 installed (`checkForUpdate` with 0.6.1 assigned).

1. **Attributes → Client** shows `mode`, `eye_color`, `msg_count`, `msg_latest`.
2. Send RPC `nextMode`. Within about 5 s `mode` follows (`EYE_ANIMATION` → `CURRENT_TIME` → `MESSAGE_HISTORY`).
3. Send RPC `addMessage` with `{"text": "hello"}`. `msg_count` goes up (max 3), `msg_latest` = `hello`.
4. Send RPC `clearMessages`. `msg_count` = 0, `msg_latest` = `-`.
5. Once the phone app exists: change the eye color over BLE and check `eye_color` follows.
6. After `reboot`, the four values match what they were before the reboot.

### 2. Confirm only after connecting

7. Right after an update reboot, `ota_awaiting_confirm` may briefly be true and `ota_image_state`
   `PENDING_VERIFY`; within seconds of the first connect they change to false / `VALID`.
8. **Timeout rollback** (needs a special build): set `OTA_TEST_SKIP_CONFIRM 1`, bump the version above
   the one on the robot, build, upload and assign it (do not commit this change). After the update,
   `ota_awaiting_confirm` stays true for about 5 minutes although the robot is online, then the robot
   reboots into the old version. Expect: `current_fw_version` back to the old one, `fw_state` = `FAILED`,
   `ota_bad_version` = the test version, `ota_last_error` mentions the rollback. `checkForUpdate` without
   `force` must not reinstall it. `checkForUpdate` with `{"force": true}` does.
9. Then upload a normal build with a higher version and check it installs and reaches `VALID`.

## Safety notes

- A broken WiFi or ThingsBoard connection in a new build causes a rollback after 5 minutes;
  a router that is off for more than 5 minutes right after an update also causes one (the version is
  then flagged bad and needs a higher version number).
