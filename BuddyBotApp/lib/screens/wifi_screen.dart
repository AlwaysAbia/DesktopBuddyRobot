import 'package:flutter/material.dart';

import '../ble/robot_connection.dart';
import 'link_banner.dart';

class WifiScreen extends StatefulWidget {
  const WifiScreen({super.key, required this.robot});
  final RobotConnection robot;

  @override
  State<WifiScreen> createState() => _WifiScreenState();
}

class _WifiScreenState extends State<WifiScreen> {
  final _ssid = TextEditingController();
  final _pass = TextEditingController();
  bool _hidePass = true;
  String? _inputError;

  RobotConnection get robot => widget.robot;

  @override
  void dispose() {
    _ssid.dispose();
    _pass.dispose();
    super.dispose();
  }

  Future<void> _send() async {
    setState(() => _inputError = null);
    try {
      await robot.sendWifi(_ssid.text, _pass.text);
    } on FormatException catch (e) {
      setState(() => _inputError = e.message);
    }
  }

  Future<void> _forget() async {
    final messenger = ScaffoldMessenger.of(context);
    final ok = await showDialog<bool>(
      context: context,
      builder: (c) => AlertDialog(
        title: const Text('Forget WiFi network?'),
        content: const Text(
            'The robot disconnects and stays offline (eye and clock still work) '
            'until you send new credentials.'),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(c, false), child: const Text('Cancel')),
          FilledButton(
              onPressed: () => Navigator.pop(c, true), child: const Text('Forget')),
        ],
      ),
    );
    if (ok != true) return;
    try {
      await robot.forgetWifi();
      messenger.showSnackBar(const SnackBar(content: Text('Network forgotten.')));
    } catch (e) {
      messenger.showSnackBar(SnackBar(content: Text('Failed: $e')));
    }
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: robot,
      builder: (context, _) {
        final busy = robot.wifiPhase == WifiPhase.sending ||
            robot.wifiPhase == WifiPhase.waiting;
        final canSend = robot.isConnected && !busy;
        return Scaffold(
          appBar: AppBar(title: const Text('WiFi setup')),
          body: Column(
            children: [
              LinkBanner(robot: robot),
              Expanded(
                child: ListView(
                  padding: const EdgeInsets.all(16),
                  children: [
                    TextField(
                      controller: _ssid,
                      enabled: !busy,
                      autocorrect: false,
                      enableSuggestions: false,
                      decoration: const InputDecoration(
                        labelText: 'Network name (SSID)',
                        helperText: '2.4 GHz networks only',
                        border: OutlineInputBorder(),
                      ),
                    ),
                    const SizedBox(height: 16),
                    TextField(
                      controller: _pass,
                      enabled: !busy,
                      obscureText: _hidePass,
                      autocorrect: false,
                      enableSuggestions: false,
                      decoration: InputDecoration(
                        labelText: 'Password',
                        helperText: 'Leave empty for an open network',
                        border: const OutlineInputBorder(),
                        suffixIcon: IconButton(
                          icon: Icon(_hidePass
                              ? Icons.visibility
                              : Icons.visibility_off),
                          onPressed: () => setState(() => _hidePass = !_hidePass),
                        ),
                      ),
                    ),
                    if (_inputError != null) ...[
                      const SizedBox(height: 8),
                      Text(_inputError!,
                          style: TextStyle(
                              color: Theme.of(context).colorScheme.error)),
                    ],
                    const SizedBox(height: 16),
                    FilledButton.icon(
                      onPressed: canSend ? _send : null,
                      icon: const Icon(Icons.send),
                      label: const Text('Send to robot'),
                    ),
                    const SizedBox(height: 16),
                    _progress(context),
                    const SizedBox(height: 24),
                    const Text(
                      'The robot replaces its stored network right away, without '
                      'testing it first. The first time, your phone may ask to pair '
                      'with the robot: accept.',
                      style: TextStyle(fontSize: 12),
                    ),
                    const SizedBox(height: 16),
                    OutlinedButton.icon(
                      onPressed: robot.isConnected && !busy ? _forget : null,
                      icon: const Icon(Icons.wifi_off),
                      label: const Text('Forget network'),
                    ),
                  ],
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _progress(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final (IconData?, Color?) style = switch (robot.wifiPhase) {
      WifiPhase.connected => (Icons.check_circle, Colors.green),
      WifiPhase.failed || WifiPhase.error => (Icons.error, scheme.error),
      WifiPhase.timeout => (Icons.hourglass_bottom, scheme.tertiary),
      _ => (null, null),
    };
    switch (robot.wifiPhase) {
      case WifiPhase.idle:
        return const SizedBox.shrink();
      case WifiPhase.sending:
      case WifiPhase.waiting:
        return Row(children: [
          const SizedBox(
              width: 20,
              height: 20,
              child: CircularProgressIndicator(strokeWidth: 2.5)),
          const SizedBox(width: 12),
          Expanded(child: Text(robot.wifiDetail)),
        ]);
      default:
        return Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Icon(style.$1, color: style.$2),
          const SizedBox(width: 12),
          Expanded(child: Text(robot.wifiDetail)),
        ]);
    }
  }
}
