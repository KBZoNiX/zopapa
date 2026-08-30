import 'dart:async';

import '../transport/robot_transport.dart';
import 'exceptions.dart';
import 'models.dart';
import 'parsers.dart';

const _defaultTimeout = Duration(seconds: 3);

/// Command/response layer over [RobotTransport]. Commands are serialized
/// (only one in flight at a time, via [_guarded]) since the firmware has no
/// way to tag which reply belongs to which request. The unsolicited run
/// summary the robot prints on every Running→Stopped transition is watched
/// independently of that queue, since it can arrive without any command
/// having asked for it.
class RobotClient {
  RobotClient(this._transport) {
    _lineSub = _transport.lines.listen(_watchForRunSummary);
  }

  final RobotTransport _transport;
  late final StreamSubscription<String> _lineSub;
  final _runSummaryController = StreamController<RunSummary>.broadcast();

  Stream<RunSummary> get runSummaries => _runSummaryController.stream;
  Stream<String> get rawLines => _transport.lines;

  bool _inRunSummaryBlock = false;
  final _runSummaryBuffer = <String>[];

  void _watchForRunSummary(String line) {
    if (!_inRunSummaryBlock) {
      if (line.trim() == '--- RESUMEN DE CORRIDA ---') {
        _inRunSummaryBlock = true;
        _runSummaryBuffer
          ..clear()
          ..add(line);
      }
      return;
    }
    _runSummaryBuffer.add(line);
    if (line.trim() == '--- FIN RESUMEN ---') {
      _inRunSummaryBlock = false;
      _runSummaryController.add(parseRunSummaryBlock(List.of(_runSummaryBuffer)));
    }
  }

  Future<void>? _queueTail;

  /// Chains [action] onto whatever is already queued, so commands never
  /// interleave their responses.
  Future<T> _guarded<T>(Future<T> Function() action) {
    final previousTail = _queueTail ?? Future<void>.value();
    final resultCompleter = Completer<T>();
    final newTail = previousTail.then((_) async {
      try {
        resultCompleter.complete(await action());
      } catch (e, st) {
        resultCompleter.completeError(e, st);
      }
    });
    _queueTail = newTail;
    return resultCompleter.future;
  }

  Future<List<String>> _collectBlock({
    required bool Function(String) isStart,
    required bool Function(String) isEnd,
    Duration timeout = _defaultTimeout,
  }) {
    final completer = Completer<List<String>>();
    final buffer = <String>[];
    var started = false;
    late StreamSubscription<String> sub;
    final timer = Timer(timeout, () {
      sub.cancel();
      if (!completer.isCompleted) {
        completer.completeError(TimeoutException('Sin respuesta del robot'));
      }
    });
    sub = _transport.lines.listen((line) {
      if (!started) {
        if (!isStart(line)) return;
        started = true;
      }
      buffer.add(line);
      if (isEnd(line)) {
        timer.cancel();
        sub.cancel();
        if (!completer.isCompleted) completer.complete(buffer);
      }
    });
    return completer.future;
  }

  Future<String> _collectLine({
    required bool Function(String) isMatch,
    Duration timeout = _defaultTimeout,
  }) async {
    final lines = await _collectBlock(isStart: isMatch, isEnd: (_) => true, timeout: timeout);
    return lines.first;
  }

  Future<void> start() => _guarded(() async {
        await _transport.write('S');
        await _collectLine(isMatch: (l) => l.trim() == 'Arrancando...');
      });

  Future<void> stop() => _guarded(() async {
        await _transport.write('P');
        await _collectLine(isMatch: (l) => l.trim() == 'Detenido');
      });

  Future<GainsStatus> readGains() => _guarded(() async {
        await _transport.write('R');
        final line =
            await _collectLine(isMatch: (l) => l.contains('KP:') && l.contains('KD:') && l.contains('Vel:'));
        return parseGainsLine(line);
      });

  Future<double> setKp(double value) => _guarded(() async {
        await _transport.write('K${_formatNumber(value)}');
        final line = await _collectLine(isMatch: (l) => l.trim().startsWith('KP ='));
        return parseAssignment(line, 'KP');
      });

  Future<double> setKd(double value) => _guarded(() async {
        await _transport.write('D${_formatNumber(value)}');
        final line = await _collectLine(isMatch: (l) => l.trim().startsWith('KD ='));
        return parseAssignment(line, 'KD');
      });

  Future<int> setSpeed(int value) => _guarded(() async {
        await _transport.write('V$value');
        final line = await _collectLine(isMatch: (l) => l.trim().startsWith('Velocidad ='));
        return parseAssignment(line, 'Velocidad').round();
      });

  Future<PresetLoadResult> loadPreset(int index) => _guarded(() async {
        await _transport.write('G$index');
        final lines = await _collectBlock(
          isStart: (l) => l.trim().startsWith('Perfil $index cargado:') || l.trim() == 'Perfil invalido, usar 1..3',
          isEnd: (l) => l.trim() == 'Perfil invalido, usar 1..3' || l.trim().startsWith('KP='),
        );
        return parsePresetLoadBlock(index, lines);
      });

  Future<PresetSaveResult> savePreset(int index) => _guarded(() async {
        await _transport.write('U$index');
        final line = await _collectLine(
          isMatch: (l) => l.trim().contains('guardados en perfil') || l.trim() == 'Perfil invalido, usar 1..3',
        );
        return parsePresetSaveLine(line);
      });

  Future<PreflightReport> preflight() => _guarded(() async {
        await _transport.write('F');
        final lines = await _collectBlock(
          isStart: (l) => l.trim() == '--- PREFLIGHT ---',
          isEnd: (l) => l.trim() == '--- FIN PREFLIGHT ---',
        );
        return parsePreflightBlock(lines);
      });

  Future<List<SensorQuality>> calibrate() => _guarded(() async {
        await _transport.write('C');
        final lines = await _collectBlock(
          isStart: (l) => l.trim() == 'Recalibrando regleta...',
          isEnd: (l) => l.trim() == 'Calibración finalizada.',
          timeout: const Duration(seconds: 6),
        );
        return parseCalibrationQuality(lines);
      });

  Future<AutotuneResult> autotune({Duration? timeout}) => _guarded(() async {
        await _transport.write('T');
        final lines = await _collectBlock(
          isStart: (l) => l.trim() == 'Iniciando AutoTune...',
          isEnd: (l) {
            final t = l.trim();
            return t == 'AutoTune Finalizado.' || t.startsWith('Autotune abortado') || t.startsWith('Autotune failed');
          },
          timeout: timeout ?? const Duration(seconds: 12),
        );
        return parseAutotuneBlock(lines);
      });

  Future<TelemetryDump> dumpTelemetry() => _guarded(() async {
        await _transport.write('Z');
        final lines = await _collectBlock(
          isStart: (l) => l.trim() == '--- TELEMETRY DUMP ---',
          isEnd: (l) => l.trim() == '--- END ---',
          timeout: const Duration(seconds: 5),
        );
        return parseTelemetryDumpBlock(lines);
      });

  Future<int> setDecimation(int n) => _guarded(() async {
        await _transport.write('N$n');
        final line = await _collectLine(isMatch: (l) => l.trim().startsWith('Decimacion telemetria ='));
        return parseAssignment(line, 'Decimacion telemetria').round();
      });

  Future<int> setRelayAmplitude(int amplitude) => _guarded(() async {
        await _transport.write('A$amplitude');
        final line = await _collectLine(isMatch: (l) => l.trim().startsWith('Amplitud rele fija ='));
        final m = RegExp(r'=\s*(-?\d+)').firstMatch(line);
        if (m == null) throw FormatException('Respuesta inesperada de amplitud: "$line"');
        return int.parse(m.group(1)!);
      });

  Future<int> setAutotuneTimeout(int ms) => _guarded(() async {
        await _transport.write('M$ms');
        final line = await _collectLine(isMatch: (l) => l.trim().startsWith('Timeout autotune ='));
        final m = RegExp(r'=\s*(\d+)\s*ms').firstMatch(line);
        if (m == null) throw FormatException('Respuesta inesperada de timeout: "$line"');
        return int.parse(m.group(1)!);
      });

  Future<GainsStatus> loadManualSlot() => _guarded(() async {
        await _transport.write('L');
        const notInitialized = 'EEPROM no inicializada. Usando valores por defecto.';
        final lines = await _collectBlock(
          isStart: (l) => l.trim().startsWith('Constantes PID cargadas') || l.trim() == notInitialized,
          isEnd: (l) => l.trim().startsWith('SPEED =') || l.trim() == notInitialized,
        );
        if (lines.first.trim() == notInitialized) {
          throw const RobotProtocolException(notInitialized);
        }
        return parseManualSlotBlock(lines);
      });

  Future<GainsStatus> saveManualSlot() => _guarded(() async {
        await _transport.write('W');
        final lines = await _collectBlock(
          isStart: (l) => l.trim().startsWith('Constantes PID guardadas'),
          isEnd: (l) => l.trim().startsWith('SPEED ='),
        );
        return parseManualSlotBlock(lines);
      });

  /// Sends an arbitrary command line without waiting for a matching reply —
  /// used by the Advanced screen's raw terminal, and for Q/O whose responses
  /// don't fit (or, for Q, may never arrive: it blocks the firmware entirely
  /// until the physical pushbutton is pressed, so waiting on this queue would
  /// hang it). Watch [rawLines] for whatever comes back.
  Future<void> sendRaw(String command) => _transport.write(command);

  void dispose() {
    _lineSub.cancel();
    _runSummaryController.close();
  }
}

String _formatNumber(double value) {
  if (value == value.roundToDouble()) return value.round().toString();
  return value.toString();
}
