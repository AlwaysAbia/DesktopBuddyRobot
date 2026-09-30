import 'package:flutter/material.dart';

import '../ble/contract.dart';
import '../ble/robot_connection.dart';
import 'link_banner.dart';
import 'wifi_screen.dart';

class ControlScreen extends StatelessWidget {
  const ControlScreen({super.key, required this.robot});
  final RobotConnection robot;

  Future<void> _run(BuildContext context, Future<void> Function() action,
      {String? ok}) async {
    final messenger = ScaffoldMessenger.of(context);
    try {
      await action();
      if (ok != null) {
        messenger
          ..hideCurrentSnackBar()
          ..showSnackBar(SnackBar(content: Text(ok)));
      }
    } catch (e) {
      messenger
        ..hideCurrentSnackBar()
        ..showSnackBar(SnackBar(content: Text('Failed: $e')));
    }
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: robot,
      builder: (context, _) {
        final enabled = robot.isConnected;
        final st = robot.status;
        return Scaffold(
          appBar: AppBar(
            title: Text(robot.deviceName),
            actions: [
              IconButton(
                tooltip: 'WiFi setup',
                icon: const Icon(Icons.wifi),
                onPressed: () => Navigator.of(context).push(MaterialPageRoute<void>(
                  builder: (_) => WifiScreen(robot: robot),
                )),
              ),
            ],
          ),
          body: Column(
            children: [
              LinkBanner(robot: robot),
              Expanded(
                child: ListView(
                  padding: const EdgeInsets.all(16),
                  children: [
                    _section(context, 'Eye color', _eyeColors(context, enabled)),
                    _section(context, 'Display mode', _modes(context, enabled)),
                    _section(
                      context,
                      'Firmware',
                      Align(
                        alignment: Alignment.centerLeft,
                        child: FilledButton.tonalIcon(
                          onPressed: enabled
                              ? () => _run(context, robot.checkForUpdate,
                                  ok: 'Update check requested. It needs the '
                                      "robot's WiFi and cloud connection.")
                              : null,
                          icon: const Icon(Icons.system_update_alt),
                          label: const Text('Check for update'),
                        ),
                      ),
                    ),
                    _section(context, 'Status', _statusPanel(st)),
                  ],
                ),
              ),
            ],
          ),
        );
      },
    );
  }

  Widget _section(BuildContext context, String title, Widget child) {
    return Card(
      margin: const EdgeInsets.only(bottom: 16),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 12),
            child,
          ],
        ),
      ),
    );
  }

  Widget _eyeColors(BuildContext context, bool enabled) {
    final selected = robot.shownEye;
    return Wrap(
      spacing: 16,
      runSpacing: 8,
      children: [
        for (final c in EyeColor.values)
          InkResponse(
            onTap: enabled
                ? () => _run(context, () => robot.setEyeColor(c.index))
                : null,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  width: 56,
                  height: 56,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: enabled ? c.color : c.color.withValues(alpha: 0.35),
                    border: Border.all(
                      width: selected == c.index ? 4 : 1,
                      color: selected == c.index
                          ? Theme.of(context).colorScheme.onSurface
                          : Colors.black26,
                    ),
                  ),
                ),
                const SizedBox(height: 4),
                Text(c.label, style: Theme.of(context).textTheme.bodySmall),
              ],
            ),
          ),
      ],
    );
  }

  Widget _modes(BuildContext context, bool enabled) {
    final m = robot.shownMode;
    return SegmentedButton<int>(
      showSelectedIcon: false,
      emptySelectionAllowed: true,
      segments: [
        for (final d in DisplayMode.values)
          ButtonSegment(value: d.index, label: Text(d.label), icon: Icon(d.icon)),
      ],
      selected: m == null ? <int>{} : {m},
      onSelectionChanged: enabled
          ? (s) => _run(context, () => robot.setMode(s.first))
          : null,
    );
  }

  Widget _statusPanel(RobotStatus? s) {
    if (s == null) return const Text('Waiting for the robot...');
    final wifi = switch (s.wifi) {
      'none' => 'No network set',
      'connecting' => 'Connecting...',
      'connected' => 'Connected',
      'failed' => 'Failed (${_errText(s.wifiErr)})',
      final other => other,
    };
    final modeName = s.mode >= 0 && s.mode < DisplayMode.values.length
        ? DisplayMode.values[s.mode].label
        : '#${s.mode}';
    Widget row(String k, String v) => Padding(
          padding: const EdgeInsets.symmetric(vertical: 3),
          child: Row(children: [
            SizedBox(width: 120, child: Text(k)),
            Expanded(child: Text(v)),
          ]),
        );
    return Column(children: [
      row('WiFi', wifi),
      row('Cloud', s.tb ? 'Connected' : 'Not connected'),
      row('Mode', modeName),
      row('Firmware', s.fw),
      if (robot.mtu != null) row('Link MTU', '${robot.mtu}'),
    ]);
  }

  String _errText(String? e) => switch (e) {
        'not_found' => 'network not found',
        'auth' => 'wrong password',
        _ => 'other',
      };
}
