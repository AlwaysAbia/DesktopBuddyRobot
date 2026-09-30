// Mirrors interface-contract.md section 1 (the contract file wins on any conflict).
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

final Guid serviceUuid = Guid('9b370000-a32f-4baf-8406-88e9298fd20d');
final Guid wifiConfigUuid = Guid('9b370001-a32f-4baf-8406-88e9298fd20d');
final Guid eyeColorUuid = Guid('9b370002-a32f-4baf-8406-88e9298fd20d');
final Guid modeUuid = Guid('9b370003-a32f-4baf-8406-88e9298fd20d');
final Guid otaTriggerUuid = Guid('9b370004-a32f-4baf-8406-88e9298fd20d');
final Guid statusUuid = Guid('9b370005-a32f-4baf-8406-88e9298fd20d');

/// Robot needs at least this MTU for WiFi Config writes and Status notifications.
const int minMtu = 128;

/// `eye::Theme` numbers.
enum EyeColor {
  cyan('Cyan', Color(0xFF00E5FF)),
  amber('Amber', Color(0xFFFFB300)),
  emerald('Emerald', Color(0xFF00C853)),
  magenta('Magenta', Color(0xFFE040FB));

  const EyeColor(this.label, this.color);
  final String label;
  final Color color;
}

/// `modes::Mode` numbers.
enum DisplayMode {
  eye('Eye', Icons.remove_red_eye_outlined),
  time('Time', Icons.access_time),
  history('History', Icons.chat_bubble_outline);

  const DisplayMode(this.label, this.icon);
  final String label;
  final IconData icon;
}

/// Parsed Status characteristic. Unknown fields are ignored (contract requirement).
class RobotStatus {
  const RobotStatus({
    required this.wifi,
    this.wifiErr,
    required this.mode,
    required this.eye,
    required this.tb,
    required this.fw,
    this.heapMinKb,
  });

  final String wifi; // none | connecting | connected | failed
  final String? wifiErr; // not_found | auth | other (only while failed)
  final int mode;
  final int eye;
  final bool tb;
  final String fw;
  final int? heapMinKb;

  /// Returns null if the bytes are not the expected JSON object.
  static RobotStatus? tryParse(List<int> bytes) {
    try {
      final j = jsonDecode(utf8.decode(bytes, allowMalformed: true));
      if (j is! Map) return null;
      return RobotStatus(
        wifi: j['wifi'] as String? ?? 'none',
        wifiErr: j['wifiErr'] as String?,
        mode: (j['mode'] as num?)?.toInt() ?? 0,
        eye: (j['eye'] as num?)?.toInt() ?? 0,
        tb: j['tb'] as bool? ?? false,
        fw: j['fw'] as String? ?? '?',
        heapMinKb: (j['heapMinKb'] as num?)?.toInt(),
      );
    } catch (_) {
      return null;
    }
  }
}

/// Encodes a WiFi Config payload. Returns the bytes, or throws [FormatException]
/// with a user-readable message if the robot would silently ignore it.
List<int> encodeWifiConfig(String ssid, String password) {
  final ssidBytes = utf8.encode(ssid).length;
  if (ssidBytes < 1) throw const FormatException('Enter the network name.');
  if (ssidBytes > 32) {
    throw const FormatException('Network name is longer than 32 bytes.');
  }
  final open = password.isEmpty;
  final hex64 = RegExp(r'^[0-9a-fA-F]{64}$').hasMatch(password);
  if (!open && !hex64 && (password.length < 8 || password.length > 63)) {
    throw const FormatException(
        'Password must be empty (open network), 8-63 characters, or 64 hex digits.');
  }
  final bytes = utf8.encode(jsonEncode({'ssid': ssid, 'password': password}));
  if (bytes.length > 200) {
    throw const FormatException('Network name and password are too long together.');
  }
  return bytes;
}

/// Payload that makes the robot forget its network.
List<int> encodeForgetWifi() => utf8.encode(jsonEncode({'ssid': ''}));
