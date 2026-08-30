import 'dart:async';

import 'package:flutter/services.dart';

import 'robot_transport.dart';

/// Classic Bluetooth SPP transport, backed by the platform channel implemented
/// in android/app/.../BtLink.kt (BluetoothSocket over the standard SPP UUID).
class AndroidBluetoothTransport implements RobotTransport {
  AndroidBluetoothTransport() {
    _eventSub = _eventChannel.receiveBroadcastStream().listen(
          _onEvent,
          onError: (_) => _setState(LinkState.disconnected),
        );
  }

  static const _methodChannel = MethodChannel('com.zopapa.control/bt');
  static const _eventChannel = EventChannel('com.zopapa.control/bt/events');

  final _linesController = StreamController<String>.broadcast();
  final _stateController = StreamController<LinkState>.broadcast();
  late final StreamSubscription<dynamic> _eventSub;
  LinkState _state = LinkState.disconnected;

  void _onEvent(dynamic event) {
    final map = Map<Object?, Object?>.from(event as Map);
    final type = map['type'] as String?;
    final data = map['data'] as String? ?? '';
    if (type == 'line') {
      _linesController.add(data);
    } else if (type == 'state') {
      _setState(data == 'connected' ? LinkState.connected : LinkState.disconnected);
    }
  }

  void _setState(LinkState state) {
    _state = state;
    _stateController.add(state);
  }

  @override
  Stream<String> get lines => _linesController.stream;

  @override
  Stream<LinkState> get connectionState => _stateController.stream;

  LinkState get currentState => _state;

  @override
  Future<List<BondedDevice>> bondedDevices() async {
    final result = await _methodChannel.invokeMethod<List<Object?>>('bondedDevices');
    return (result ?? const [])
        .map((e) => Map<Object?, Object?>.from(e as Map))
        .map((m) => BondedDevice(name: m['name']! as String, address: m['address']! as String))
        .toList();
  }

  @override
  Future<void> connect(String address) async {
    _setState(LinkState.connecting);
    try {
      await _methodChannel.invokeMethod('connect', {'address': address});
      _setState(LinkState.connected);
    } catch (_) {
      _setState(LinkState.disconnected);
      rethrow;
    }
  }

  @override
  Future<void> disconnect() async {
    await _methodChannel.invokeMethod('disconnect');
    _setState(LinkState.disconnected);
  }

  @override
  Future<void> write(String line) {
    return _methodChannel.invokeMethod('write', {'line': line});
  }

  void dispose() {
    _eventSub.cancel();
    _linesController.close();
    _stateController.close();
  }
}
