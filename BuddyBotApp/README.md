# BuddyBotApp

Flutter companion app (Android + iOS). It talks to the robot only over BLE, using
[../interface-contract.md](../interface-contract.md) section 1 as the ground truth. No internet or
server calls.

## One-time setup

The `android/` and `ios/` folders are not in the repo yet (Flutter was not installed when the app
code was written). Generate them once:

```bash
cd BuddyBotApp
flutter create --platforms=android,ios --org com.buddybot --project-name buddybot_app .
git status   # lib/, test/, pubspec.yaml must show no changes; restore them if flutter create touched them
flutter pub get
flutter test
```

Then add the permissions below.

### Android (`android/app/src/main/AndroidManifest.xml`, inside `<manifest>`, before `<application>`)

```xml
<uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
<uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
```

Set `minSdk` to at least 21 in `android/app/build.gradle(.kts)` (the Flutter default is fine).

### iOS (`ios/Runner/Info.plist`, inside the top `<dict>`)

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>BuddyBot uses Bluetooth to find and control your robot.</string>
```

iOS deployment target 12.0 or higher. BLE does not work in the iOS simulator; use a real device.

## Run

```bash
flutter run            # with a phone connected
```

## Layout

- `lib/ble/contract.dart`: UUIDs, eye colors, modes, Status parsing, WiFi payload encoding and validation.
- `lib/ble/robot_connection.dart`: connect, discovery, MTU, Status subscription, auto-reconnect, WiFi setup progress.
- `lib/screens/`: scan, control, WiFi setup.
- `test/contract_test.dart`: parsing and payload validation (the only part covered by unit tests).
