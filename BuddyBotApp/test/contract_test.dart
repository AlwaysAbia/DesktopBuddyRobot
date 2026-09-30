import 'dart:convert';

import 'package:buddybot_app/ble/contract.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('RobotStatus.tryParse', () {
    test('parses the contract example and ignores unknown fields', () {
      final s = RobotStatus.tryParse(utf8.encode(
          '{"wifi":"connected","mode":0,"eye":1,"tb":true,"fw":"0.5.0","heapMinKb":58,"new":1}'))!;
      expect(s.wifi, 'connected');
      expect(s.wifiErr, isNull);
      expect(s.eye, 1);
      expect(s.tb, true);
      expect(s.fw, '0.5.0');
      expect(s.heapMinKb, 58);
    });

    test('reads wifiErr', () {
      final s = RobotStatus.tryParse(
          utf8.encode('{"wifi":"failed","wifiErr":"auth","mode":2,"eye":0,"tb":false,"fw":"x"}'))!;
      expect(s.wifiErr, 'auth');
    });

    test('garbage gives null', () {
      expect(RobotStatus.tryParse(utf8.encode('not json')), isNull);
      expect(RobotStatus.tryParse([]), isNull);
      expect(RobotStatus.tryParse(utf8.encode('[1]')), isNull);
    });
  });

  group('encodeWifiConfig', () {
    test('valid', () {
      final j = jsonDecode(utf8.decode(encodeWifiConfig('Home', 'secret123')));
      expect(j, {'ssid': 'Home', 'password': 'secret123'});
    });
    test('open network', () => expect(() => encodeWifiConfig('Cafe', ''), returnsNormally));
    test('64 hex key', () =>
        expect(() => encodeWifiConfig('Home', 'a' * 64), returnsNormally));
    test('rejects empty ssid', () =>
        expect(() => encodeWifiConfig('', 'secret123'), throwsFormatException));
    test('rejects ssid over 32 bytes', () =>
        expect(() => encodeWifiConfig('é' * 17, 'secret123'), throwsFormatException));
    test('rejects short password', () =>
        expect(() => encodeWifiConfig('Home', 'short'), throwsFormatException));
    test('rejects 64 non-hex chars', () =>
        expect(() => encodeWifiConfig('Home', 'z' * 64), throwsFormatException));
    test('forget payload', () =>
        expect(utf8.decode(encodeForgetWifi()), '{"ssid":""}'));
  });
}
