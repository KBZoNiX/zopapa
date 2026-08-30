/// Thrown when the robot itself reports a protocol-level rejection
/// (e.g. "Perfil invalido, usar 1..3"), as opposed to a transport/timeout error.
class RobotProtocolException implements Exception {
  const RobotProtocolException(this.message);

  final String message;

  @override
  String toString() => message;
}
