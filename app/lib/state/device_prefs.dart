import 'package:shared_preferences/shared_preferences.dart';

class DevicePrefs {
  static const _lastDeviceKey = 'last_device_address';

  Future<String?> getLastDeviceAddress() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_lastDeviceKey);
  }

  Future<void> setLastDeviceAddress(String address) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_lastDeviceKey, address);
  }
}
