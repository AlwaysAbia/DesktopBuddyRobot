import 'dart:async';
import 'dart:io' show Platform;

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../ble/contract.dart';
import '../ble/robot_connection.dart';
import 'control_screen.dart';

class ScanScreen extends StatefulWidget {
  const ScanScreen({super.key, required this.robot});
  final RobotConnection robot;

  @override
  State<ScanScreen> createState() => _ScanScreenState();
}

class _ScanScreenState extends State<ScanScreen> {
  BluetoothAdapterState _adapter = BluetoothAdapterState.unknown;
  List<ScanResult> _results = [];
  bool _scanning = false;
  DeviceIdentifier? _connectingId;
  final _subs = <StreamSubscription<dynamic>>[];

  @override
  void initState() {
    super.initState();
    _subs.add(FlutterBluePlus.adapterState.listen((s) {
      if (!mounted) return;
      setState(() => _adapter = s);
      if (s == BluetoothAdapterState.on && !_scanning && _connectingId == null) {
        _scan();
      }
    }));
    _subs.add(FlutterBluePlus.scanResults.listen((r) {
      if (mounted) setState(() => _results = r);
    }));
    _subs.add(FlutterBluePlus.isScanning.listen((v) {
      if (mounted) setState(() => _scanning = v);
    }));
  }

  @override
  void dispose() {
    for (final s in _subs) {
      s.cancel();
    }
    FlutterBluePlus.stopScan();
    super.dispose();
  }

  Future<void> _scan() async {
    try {
      // The service UUID is in the advertising packet (contract), so filter on it.
      await FlutterBluePlus.startScan(
        withServices: [serviceUuid],
        timeout: const Duration(seconds: 12),
      );
    } catch (e) {
      _snack('Scan failed: $e');
    }
  }

  Future<void> _connect(ScanResult r) async {
    await FlutterBluePlus.stopScan();
    setState(() => _connectingId = r.device.remoteId);
    try {
      await widget.robot.connect(r.device);
      if (!mounted) return;
      await Navigator.of(context).push(MaterialPageRoute<void>(
        builder: (_) => ControlScreen(robot: widget.robot),
      ));
      await widget.robot.disconnect();
    } catch (e) {
      _snack('Could not connect: ${widget.robot.linkError ?? e}');
    } finally {
      if (mounted) {
        setState(() => _connectingId = null);
        _scan();
      }
    }
  }

  void _snack(String msg) {
    if (!mounted) return;
    ScaffoldMessenger.of(context)
      ..hideCurrentSnackBar()
      ..showSnackBar(SnackBar(content: Text(msg)));
  }

  @override
  Widget build(BuildContext context) {
    final on = _adapter == BluetoothAdapterState.on;
    return Scaffold(
      appBar: AppBar(
        title: const Text('Find your BuddyBot'),
        bottom: _scanning
            ? const PreferredSize(
                preferredSize: Size.fromHeight(3),
                child: LinearProgressIndicator(minHeight: 3),
              )
            : null,
      ),
      body: !on ? _adapterOff() : _list(),
      floatingActionButton: on && !_scanning && _connectingId == null
          ? FloatingActionButton.extended(
              onPressed: _scan,
              icon: const Icon(Icons.search),
              label: const Text('Scan again'),
            )
          : null,
    );
  }

  Widget _adapterOff() {
    final unsupported = _adapter == BluetoothAdapterState.unavailable;
    final unauthorized = _adapter == BluetoothAdapterState.unauthorized;
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.bluetooth_disabled, size: 64),
            const SizedBox(height: 16),
            Text(
              unsupported
                  ? 'This device has no Bluetooth LE.'
                  : unauthorized
                      ? 'Bluetooth permission was denied. Allow it for this app in the system settings.'
                      : 'Bluetooth is off.',
              textAlign: TextAlign.center,
            ),
            if (!unsupported && !unauthorized && Platform.isAndroid) ...[
              const SizedBox(height: 16),
              FilledButton(
                onPressed: () => FlutterBluePlus.turnOn(),
                child: const Text('Turn on Bluetooth'),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _list() {
    if (_results.isEmpty) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Text(
            _scanning
                ? 'Looking for a BuddyBot nearby...'
                : 'No BuddyBot found. Check that it is powered and within a few metres, '
                    'and that no other phone is connected to it (it accepts one at a time).',
            textAlign: TextAlign.center,
          ),
        ),
      );
    }
    return ListView(
      children: [
        for (final r in _results)
          ListTile(
            leading: const Icon(Icons.smart_toy_outlined),
            title: Text(r.advertisementData.advName.isNotEmpty
                ? r.advertisementData.advName
                : r.device.platformName.isNotEmpty
                    ? r.device.platformName
                    : 'BuddyBot'),
            subtitle: Text('Signal ${r.rssi} dBm'),
            trailing: _connectingId == r.device.remoteId
                ? const SizedBox(
                    width: 24,
                    height: 24,
                    child: CircularProgressIndicator(strokeWidth: 2.5),
                  )
                : const Icon(Icons.chevron_right),
            enabled: _connectingId == null,
            onTap: () => _connect(r),
          ),
      ],
    );
  }
}
