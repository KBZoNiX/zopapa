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
        theme: _zopapaTheme(),
        darkTheme: _zopapaTheme(),
        themeMode: ThemeMode.dark,
        home: const _RootScreen(),
      ),
    );
  }
}

// Matches the robot artwork: red/yellow/blue accents on a black ground.
// Always dark — the branding doesn't adapt to system light mode.
ThemeData _zopapaTheme() {
  final colorScheme = ColorScheme.fromSeed(
    seedColor: const Color(0xFFE5342A),
    brightness: Brightness.dark,
  ).copyWith(
    primary: const Color(0xFFE5342A),
    secondary: const Color(0xFFFFC72C),
    // fromSeed derives secondaryContainer from the red seed, not the
    // yellow override above — set it explicitly so the active-preset
    // highlight actually reads as yellow instead of a muted red.
    secondaryContainer: const Color(0xFF4A3B00),
    onSecondaryContainer: const Color(0xFFFFE082),
    tertiary: const Color(0xFF2D7DD2),
    surface: const Color(0xFF141414),
  );
  return ThemeData(
    colorScheme: colorScheme,
    useMaterial3: true,
    scaffoldBackgroundColor: Colors.black,
    appBarTheme: const AppBarTheme(backgroundColor: Colors.black, foregroundColor: Colors.white),
    // Bordered/secondary actions (Preflight, Calibrar, AutoTune, Advanced
    // screen) get the blue accent so they read as distinct from the red
    // filled CTAs (ARRANCAR, Cargar) without adding more colors.
    outlinedButtonTheme: OutlinedButtonThemeData(
      style: OutlinedButton.styleFrom(
        foregroundColor: colorScheme.tertiary,
        side: BorderSide(color: colorScheme.tertiary),
      ),
    ),
  );
}

class _RootScreen extends StatelessWidget {
  const _RootScreen();

  @override
  Widget build(BuildContext context) {
    final state = context.watch<RobotState>();
    return state.isConnected ? const HomeScreen() : const ConnectScreen();
  }
}
