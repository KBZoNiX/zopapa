import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'protocol/robot_client.dart';
import 'screens/connect_screen.dart';
import 'screens/home_screen.dart';
import 'state/device_prefs.dart';
import 'state/robot_state.dart';
import 'transport/android_bluetooth_transport.dart';

void main() {
  runApp(const ZopapaControlApp());
}

class ZopapaControlApp extends StatefulWidget {
  const ZopapaControlApp({super.key});

  @override
  State<ZopapaControlApp> createState() => _ZopapaControlAppState();
}

class _ZopapaControlAppState extends State<ZopapaControlApp> {
  late final AndroidBluetoothTransport _transport;
  late final RobotClient _client;
  late final RobotState _robotState;

  @override
  void initState() {
    super.initState();
    _transport = AndroidBluetoothTransport();
    _client = RobotClient(_transport);
    _robotState = RobotState(client: _client, transport: _transport, devicePrefs: DevicePrefs());
  }

  @override
  void dispose() {
    _robotState.dispose();
    _client.dispose();
    _transport.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider.value(
      value: _robotState,
      child: MaterialApp(
        title: 'Zopapa Control',
        theme: ThemeData(colorSchemeSeed: Colors.deepOrange, useMaterial3: true, brightness: Brightness.light),
        darkTheme: ThemeData(colorSchemeSeed: Colors.deepOrange, useMaterial3: true, brightness: Brightness.dark),
        home: const _RootScreen(),
      ),
    );
  }
}

class _RootScreen extends StatelessWidget {
  const _RootScreen();

  @override
  Widget build(BuildContext context) {
    final state = context.watch<RobotState>();
    return state.isConnected ? const HomeScreen() : const ConnectScreen();
  }
}
