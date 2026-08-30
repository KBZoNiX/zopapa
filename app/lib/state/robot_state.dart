import 'dart:async';

import 'package:flutter/foundation.dart';

import '../protocol/exceptions.dart';
import '../protocol/models.dart';
import '../protocol/robot_client.dart';
import '../transport/robot_transport.dart';
import 'device_prefs.dart';

/// App-wide connection/robot state. There is no firmware command to ask
/// "are you running right now" (only Stop is accepted while running at
/// all), so [isRunning] is tracked client-side: set on Start/open-loop-drive,
/// cleared on a successful Stop or on any unsolicited run summary (which
/// fires on every Running→Stopped transition, including auto-stops this app
/// didn't initiate).
class RobotState extends ChangeNotifier {
  RobotState({
    required RobotClient client,
    required RobotTransport transport,
    required DevicePrefs devicePrefs,
  })  : _client = client,
        _transport = transport,
        _devicePrefs = devicePrefs {
    _connSub = _transport.connectionState.listen(_onConnectionState);
    _runSummarySub = _client.runSummaries.listen(_onRunSummary);
  }

  final RobotClient _client;
  final RobotTransport _transport;
  final DevicePrefs _devicePrefs;
  late final StreamSubscription<LinkState> _connSub;
  late final StreamSubscription<RunSummary> _runSummarySub;

  RobotClient get client => _client;

  LinkState _linkState = LinkState.disconnected;
  LinkState get linkState => _linkState;
  bool get isConnected => _linkState == LinkState.connected;

  bool isRunning = false;
  GainsStatus? lastGains;
  int? activePreset;
  PreflightReport? lastPreflight;
  RunSummary? lastRunSummary;
  String? lastError;

  void _onConnectionState(LinkState state) {
    _linkState = state;
    if (state != LinkState.connected) isRunning = false;
    notifyListeners();
  }

  void _onRunSummary(RunSummary summary) {
    isRunning = false;
    lastRunSummary = summary;
    lastGains = GainsStatus(kp: summary.kp, kd: summary.kd, speed: summary.speed);
    activePreset = summary.activePreset;
    notifyListeners();
  }

  Future<List<BondedDevice>> bondedDevices() => _transport.bondedDevices();

  Future<String?> lastDeviceAddress() => _devicePrefs.getLastDeviceAddress();

  Future<void> connect(String address) async {
    lastError = null;
    try {
      await _transport.connect(address);
      await _devicePrefs.setLastDeviceAddress(address);
      await refreshGains();
    } catch (e) {
      lastError = e.toString();
      notifyListeners();
      rethrow;
    }
  }

  Future<void> disconnect() => _transport.disconnect();

  Future<void> start() => _guardedRun(() async {
        await _client.start();
        isRunning = true;
      });

  Future<void> stop() => _guardedRun(() async {
        await _client.stop();
        isRunning = false;
      });

  /// Straight/open-loop drive (Advanced screen only) — has no line-loss
  /// safety stop, only Stop ends it.
  Future<void> driveStraightOpenLoop() => _guardedRun(() async {
        await _client.sendRaw('O');
        isRunning = true;
      });

  Future<void> refreshGains() => _guardedRun(() async {
        lastGains = await _client.readGains();
      });

  Future<void> loadPreset(int index) => _guardedRun(() async {
        final result = await _client.loadPreset(index);
        lastGains = GainsStatus(kp: result.kp, kd: result.kd, speed: result.speed);
        activePreset = index;
      });

  Future<void> savePreset(int index) => _guardedRun(() async {
        await _client.savePreset(index);
        activePreset = index;
      });

  Future<void> setKp(double value) => _guardedRun(() async {
        final kp = await _client.setKp(value);
        lastGains = (lastGains ?? const GainsStatus(kp: 0, kd: 0, speed: 0)).copyWith(kp: kp);
        activePreset = null;
      });

  Future<void> setKd(double value) => _guardedRun(() async {
        final kd = await _client.setKd(value);
        lastGains = (lastGains ?? const GainsStatus(kp: 0, kd: 0, speed: 0)).copyWith(kd: kd);
        activePreset = null;
      });

  Future<void> setSpeed(int value) => _guardedRun(() async {
        final speed = await _client.setSpeed(value);
        lastGains = (lastGains ?? const GainsStatus(kp: 0, kd: 0, speed: 0)).copyWith(speed: speed);
        activePreset = null;
      });

  Future<void> runPreflight() => _guardedRun(() async {
        lastPreflight = await _client.preflight();
        activePreset = lastPreflight!.activePreset;
      });

  Future<void> calibrate() => _guardedRun(() => _client.calibrate());

  Future<AutotuneResult> autotune() async {
    late final AutotuneResult result;
    await _guardedRun(() async {
      result = await _client.autotune(
        timeout: lastPreflight != null ? Duration(milliseconds: lastPreflight!.autotuneTimeoutMs + 4000) : null,
      );
      if (result.ok) {
        lastGains = (lastGains ?? const GainsStatus(kp: 0, kd: 0, speed: 0))
            .copyWith(kp: result.installedKp, kd: result.installedKd);
        activePreset = null;
      }
    });
    return result;
  }

  Future<T> _guardedRun<T>(Future<T> Function() action) async {
    lastError = null;
    try {
      final result = await action();
      notifyListeners();
      return result;
    } catch (e) {
      lastError = e is RobotProtocolException ? e.message : e.toString();
      notifyListeners();
      rethrow;
    }
  }

  @override
  void dispose() {
    _connSub.cancel();
    _runSummarySub.cancel();
    super.dispose();
  }
}
