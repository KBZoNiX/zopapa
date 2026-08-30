import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:provider/provider.dart';

import '../state/robot_state.dart';
import '../transport/robot_transport.dart';

class ConnectScreen extends StatefulWidget {
  const ConnectScreen({super.key});

  @override
  State<ConnectScreen> createState() => _ConnectScreenState();
}

class _ConnectScreenState extends State<ConnectScreen> {
  List<BondedDevice>? _devices;
  String? _lastAddress;
  String? _error;
  String? _connectingAddress;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    final status = await Permission.bluetoothConnect.request();
    if (!mounted) return;
    if (!status.isGranted) {
      setState(() => _error = 'Se necesita el permiso de Bluetooth para continuar.');
      return;
    }

    final state = context.read<RobotState>();
    try {
      final devices = await state.bondedDevices();
      final last = await state.lastDeviceAddress();
      if (!mounted) return;
      setState(() {
        _devices = devices;
        _lastAddress = last;
        _error = null;
      });
    } catch (e) {
      if (!mounted) return;
      setState(() => _error = e.toString());
    }
  }

  Future<void> _connect(String address) async {
    setState(() {
      _connectingAddress = address;
      _error = null;
    });
    try {
      await context.read<RobotState>().connect(address);
    } catch (e) {
      if (!mounted) return;
      setState(() => _error = 'No se pudo conectar: $e');
    } finally {
      if (mounted) setState(() => _connectingAddress = null);
    }
  }

  @override
  Widget build(BuildContext context) {
    final devices = _devices;
    return Scaffold(
      appBar: AppBar(title: const Text('Zopapa Control')),
      body: RefreshIndicator(
        onRefresh: _load,
        child: devices == null
            ? const Center(child: CircularProgressIndicator())
            : ListView(
                children: [
                  if (_error != null)
                    Padding(
                      padding: const EdgeInsets.all(16),
                      child: Text(_error!, style: TextStyle(color: Theme.of(context).colorScheme.error)),
                    ),
                  if (devices.isEmpty)
                    const Padding(
                      padding: EdgeInsets.all(16),
                      child: Text(
                        'No hay dispositivos Bluetooth emparejados. Emparejá el módulo del robot '
                        'desde los ajustes de Bluetooth de Android primero (una sola vez).',
                      ),
                    ),
                  for (final device in devices)
                    ListTile(
                      leading: Icon(device.address == _lastAddress ? Icons.star : Icons.bluetooth),
                      title: Text(device.name),
                      subtitle: Text(device.address),
                      trailing: _connectingAddress == device.address
                          ? const SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2))
                          : null,
                      onTap: _connectingAddress == null ? () => _connect(device.address) : null,
                    ),
                ],
              ),
      ),
    );
  }
}
