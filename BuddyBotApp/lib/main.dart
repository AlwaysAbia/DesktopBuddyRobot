import 'package:flutter/material.dart';

import 'ble/robot_connection.dart';
import 'screens/scan_screen.dart';

void main() => runApp(const BuddyApp());

class BuddyApp extends StatefulWidget {
  const BuddyApp({super.key});

  @override
  State<BuddyApp> createState() => _BuddyAppState();
}

class _BuddyAppState extends State<BuddyApp> {
  final _robot = RobotConnection();

  @override
  void dispose() {
    _robot.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BuddyBot',
      theme: ThemeData(
        colorSchemeSeed: const Color(0xFF00B8D4),
        useMaterial3: true,
      ),
      darkTheme: ThemeData(
        colorSchemeSeed: const Color(0xFF00B8D4),
        brightness: Brightness.dark,
        useMaterial3: true,
      ),
      home: ScanScreen(robot: _robot),
    );
  }
}
