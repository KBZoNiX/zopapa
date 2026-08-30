/// Transport-layer seam. [AndroidBluetoothTransport] is the only implementation
/// today; a future Linux/BlueZ backend would implement this same interface so
/// nothing above it (protocol/UI) needs to change.
enum LinkState { disconnected, connecting, connected }

class BondedDevice {
  const BondedDevice({required this.name, required this.address});

  final String name;
  final String address;
}

abstract class RobotTransport {
  Stream<String> get lines;
  Stream<LinkState> get connectionState;

  Future<List<BondedDevice>> bondedDevices();
  Future<void> connect(String address);
  Future<void> disconnect();
  Future<void> write(String line);
}
