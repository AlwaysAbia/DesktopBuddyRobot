import 'package:flutter/material.dart';

import '../ble/robot_connection.dart';

/// Shows reconnect progress / a one-tap retry when the BLE link drops.
class LinkBanner extends StatelessWidget {
  const LinkBanner({super.key, required this.robot});
  final RobotConnection robot;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    switch (robot.link) {
      case LinkState.reconnecting:
        return Material(
          color: scheme.tertiaryContainer,
          child: const Padding(
            padding: EdgeInsets.all(12),
            child: Row(children: [
              SizedBox(
                  width: 18,
                  height: 18,
                  child: CircularProgressIndicator(strokeWidth: 2)),
              SizedBox(width: 12),
              Expanded(child: Text('Connection dropped. Reconnecting...')),
            ]),
          ),
        );
      case LinkState.lost:
        return Material(
          color: scheme.errorContainer,
          child: Padding(
            padding: const EdgeInsets.fromLTRB(12, 4, 4, 4),
            child: Row(children: [
              Expanded(
                  child: Text(robot.linkError ?? 'Connection to the robot lost.')),
              TextButton(onPressed: robot.retry, child: const Text('Retry')),
            ]),
          ),
        );
      default:
        return const SizedBox.shrink();
    }
  }
}
