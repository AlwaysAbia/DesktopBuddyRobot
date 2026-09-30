import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'contract.dart';

enum LinkState { idle, connecting, connected, reconnecting, lost }

enum WifiPhase { idle, sending, waiting, connected, failed, timeout, error }

/// Owns the BLE connection to one robot: connect, discovery, MTU, Status
/// subscription, auto-reconnect and the WiFi-setup progress state.
class RobotConnection extends ChangeNotifier {
  static const _reconnectDelays = [1, 2, 3, 5, 8, 10]; // seconds
  static const _wifiOverallTimeout = Duration(seconds: 40); // robot gives up after 20 s
  static const _wifiGrace = Duration(seconds: 6);

  BluetoothDevice? _device;
  StreamSubscription<BluetoothConnectionState>? _stateSub;
  StreamSubscription<List<int>>? _statusSub;
  Timer? _poll;
  bool _userDisconnect = false;
  bool _reconnecting = false;

  BluetoothCharacteristic? _wifiChar, _eyeChar, _modeChar, _otaChar, _statusChar;

  LinkState link = LinkState.idle;
  String? linkError;
  RobotStatus? status;
  int? mtu;

  /// Set while a write is in flight so the UI reflects the tap immediately.
  int? pendingEye, pendingMode;

  WifiPhase wifiPhase = WifiPhase.idle;
  String wifiDetail = '';
  String? _wifiSsid;
  bool _sawConnecting = false, _graceOver = false;
  Timer? _wifiTimeout, _wifiGraceTimer;

  String get deviceName {
    final n = _device?.platformName ?? '';
    return n.isEmpty ? 'BuddyBot' : n;
  }

  bool get isConnected => link == LinkState.connected;
  int? get shownEye => pendingEye ?? status?.eye;
  int? get shownMode => pendingMode ?? status?.mode;

  // ---------------------------------------------------------------- connect

  /// First connection from the scan screen. Throws on failure.
  Future<void> connect(BluetoothDevice device) async {
    await _teardown(disconnectDevice: true);
    _device = device;
    _userDisconnect = false;
    _setLink(LinkState.connecting);
    try {
      await _open();
    } catch (e) {
      await _teardown(disconnectDevice: true);
      _setLink(LinkState.idle, error: _describe(e));
      rethrow;
    }
  }

  /// Leaves the robot on purpose (no auto-reconnect).
  Future<void> disconnect() async {
    _userDisconnect = true;
    _resetWifi();
    await _teardown(disconnectDevice: true);
    status = null;
    _setLink(LinkState.idle);
  }

  /// One-tap retry after the app gave up reconnecting.
  void retry() {
    if (_device == null || _reconnecting) return;
    _userDisconnect = false;
    unawaited(_reconnectLoop());
  }

  Future<void> _open() async {
    final d = _device!;
    await d.connect(timeout: const Duration(seconds: 15), mtu: 247);

    _stateSub ??= d.connectionState.listen((s) {
      if (s == BluetoothConnectionState.disconnected &&
          link == LinkState.connected &&
          !_userDisconnect) {
        _onDropped();
      }
    });

    final services = await d.discoverServices();
    final service = services.where((s) => s.uuid == serviceUuid).firstOrNull;
    if (service == null) {
      throw StateError('This device does not offer the BuddyBot service.');
    }
    BluetoothCharacteristic? find(Guid u) =>
        service.characteristics.where((c) => c.uuid == u).firstOrNull;
    _wifiChar = find(wifiConfigUuid);
    _eyeChar = find(eyeColorUuid);
    _modeChar = find(modeUuid);
    _otaChar = find(otaTriggerUuid);
    _statusChar = find(statusUuid);
    if ([_wifiChar, _eyeChar, _modeChar, _otaChar, _statusChar].contains(null)) {
      throw StateError('Robot firmware is missing BLE characteristics (too old?).');
    }

    // Contract: >= 128 bytes before WiFi writes / Status notifications.
    // iOS negotiates by itself; Android was asked in connect().
    if (d.mtuNow < minMtu) {
      try {
        await d.requestMtu(247);
      } catch (_) {}
    }
    mtu = d.mtuNow;

    await _statusSub?.cancel();
    _statusSub = _statusChar!.onValueReceived.listen(_applyStatus);
    _poll?.cancel();
    if (mtu! >= minMtu) {
      await _statusChar!.setNotifyValue(true);
    } else {
      // Notifications would be dropped by the robot; reads still work.
      _poll = Timer.periodic(const Duration(seconds: 3), (_) => _refresh());
    }
    _applyStatus(await _statusChar!.read());

    linkError = null;
    _setLink(LinkState.connected);
    _evaluateWifi();
  }

  void _onDropped() {
    _poll?.cancel();
    unawaited(_reconnectLoop());
  }

  Future<void> _reconnectLoop() async {
    if (_reconnecting) return;
    _reconnecting = true;
    _setLink(LinkState.reconnecting);
    try {
      for (final secs in _reconnectDelays) {
        await Future<void>.delayed(Duration(seconds: secs));
        if (_userDisconnect || _device == null) return;
        try {
          await _open();
          return;
        } catch (_) {
          // try again after the next delay
        }
      }
      if (!_userDisconnect) {
        _setLink(LinkState.lost, error: 'Lost connection to the robot.');
      }
    } finally {
      _reconnecting = false;
    }
  }

  Future<void> _teardown({required bool disconnectDevice}) async {
    _poll?.cancel();
    await _statusSub?.cancel();
    _statusSub = null;
    await _stateSub?.cancel();
    _stateSub = null;
    final d = _device;
    if (disconnectDevice && d != null) {
      try {
        await d.disconnect();
      } catch (_) {}
    }
  }

  // ----------------------------------------------------------------- status

  void _applyStatus(List<int> bytes) {
    final s = RobotStatus.tryParse(bytes);
    if (s == null) return;
    status = s;
    pendingEye = null;
    pendingMode = null;
    _evaluateWifi();
    notifyListeners();
  }

  Future<void> _refresh() async {
    final c = _statusChar;
    if (c == null || !isConnected) return;
    try {
      _applyStatus(await c.read());
    } catch (_) {}
  }

  // ----------------------------------------------------------------- writes

  Future<void> setEyeColor(int value) async {
    pendingEye = value;
    notifyListeners();
    try {
      await _eyeChar!.write([value], withoutResponse: false);
      await _refresh();
    } finally {
      // Status (notification or read) clears the pending value; if that failed
      // the UI falls back to the last known robot value.
      pendingEye = null;
      notifyListeners();
    }
  }

  Future<void> setMode(int value) async {
    pendingMode = value;
    notifyListeners();
    try {
      await _modeChar!.write([value], withoutResponse: false);
      await _refresh();
    } finally {
      pendingMode = null;
      notifyListeners();
    }
  }

  /// Any payload works per the contract; one byte avoids odd 0-byte-write behavior.
  Future<void> checkForUpdate() async {
    await _otaChar!.write([1], withoutResponse: false);
    await _refresh();
  }

  /// Sends new WiFi credentials and tracks the outcome through Status.
  /// Throws [FormatException] for input the robot would silently ignore.
  Future<void> sendWifi(String ssid, String password) async {
    final bytes = encodeWifiConfig(ssid, password);
    _resetWifi();
    _wifiSsid = ssid;
    wifiPhase = WifiPhase.sending;
    wifiDetail = 'Sending to the robot...';
    notifyListeners();
    try {
      await _writeWifi(bytes);
    } catch (e) {
      wifiPhase = WifiPhase.error;
      wifiDetail = 'Could not send: ${_describe(e)}';
      notifyListeners();
      return;
    }
    wifiPhase = WifiPhase.waiting;
    wifiDetail = 'Robot is trying to join "$ssid"...';
    _sawConnecting = false;
    _graceOver = false;
    _wifiGraceTimer = Timer(_wifiGrace, () {
      _graceOver = true;
      unawaited(_refresh().then((_) => _evaluateWifi()));
    });
    _wifiTimeout = Timer(_wifiOverallTimeout, () {
      if (wifiPhase != WifiPhase.waiting) return;
      wifiPhase = WifiPhase.timeout;
      wifiDetail = 'No result yet. The robot may still be trying; '
          'it retries every 30 s. Check the status on the control screen.';
      notifyListeners();
    });
    notifyListeners();
    unawaited(_refresh());
  }

  /// Empty SSID = the robot forgets its network and stays offline.
  Future<void> forgetWifi() async {
    _resetWifi();
    await _writeWifi(encodeForgetWifi());
    await _refresh();
  }

  /// WiFi Config needs an encrypted link, so the first write triggers the OS
  /// pairing prompt. The first attempt can fail while that completes; retry once.
  Future<void> _writeWifi(List<int> bytes) async {
    for (var attempt = 0;; attempt++) {
      try {
        await _wifiChar!.write(bytes, withoutResponse: false, timeout: 45);
        return;
      } on FlutterBluePlusException {
        if (attempt >= 1 || !isConnected) rethrow;
        await Future<void>.delayed(const Duration(seconds: 3));
      }
    }
  }

  void _evaluateWifi() {
    if (wifiPhase != WifiPhase.waiting) return;
    final s = status;
    if (s == null) return;
    if (s.wifi == 'connecting') {
      _sawConnecting = true;
      return;
    }
    // A stale "connected"/"failed" from before the write must not count, so a
    // result only counts after we saw "connecting" or the grace period passed.
    if (!(_sawConnecting || _graceOver)) return;
    if (s.wifi == 'connected') {
      _finishWifi(WifiPhase.connected, 'Robot joined "$_wifiSsid".');
    } else if (s.wifi == 'failed') {
      final why = switch (s.wifiErr) {
        'not_found' => 'network not found (the robot only supports 2.4 GHz)',
        'auth' => 'wrong password',
        _ => 'connection failed',
      };
      _finishWifi(WifiPhase.failed,
          'Could not join "$_wifiSsid": $why. The robot keeps these '
          'settings and retries every 30 s; send them again to correct them.');
    }
  }

  void _finishWifi(WifiPhase phase, String detail) {
    _wifiTimeout?.cancel();
    _wifiGraceTimer?.cancel();
    wifiPhase = phase;
    wifiDetail = detail;
  }

  void _resetWifi() {
    _wifiTimeout?.cancel();
    _wifiGraceTimer?.cancel();
    wifiPhase = WifiPhase.idle;
    wifiDetail = '';
  }

  // ---------------------------------------------------------------- helpers

  void _setLink(LinkState s, {String? error}) {
    link = s;
    if (error != null) linkError = error;
    notifyListeners();
  }

  String _describe(Object e) {
    if (e is FlutterBluePlusException) return e.description ?? 'BLE error ${e.code}';
    if (e is StateError) return e.message;
    return e.toString();
  }

  @override
  void dispose() {
    _wifiTimeout?.cancel();
    _wifiGraceTimer?.cancel();
    unawaited(_teardown(disconnectDevice: true));
    super.dispose();
  }
}
