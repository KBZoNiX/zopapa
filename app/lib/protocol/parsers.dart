/// Parsers for the plain-Spanish-text responses documented in bt.ino.
/// Each function takes exactly the line(s) belonging to one response — the
/// caller (RobotClient) is responsible for slicing those out of the raw
/// line stream by matching the start/end markers or exact strings below.
library;

import 'exceptions.dart';
import 'models.dart';

const _numPattern = r'-?\d+(?:\.\d+)?';

GainsStatus parseGainsLine(String line) {
  final re = RegExp('KP:\\s*($_numPattern)\\s*\\|\\s*KD:\\s*($_numPattern)\\s*\\|\\s*Vel:\\s*($_numPattern)');
  final m = re.firstMatch(line);
  if (m == null) {
    throw FormatException('No se pudo interpretar la respuesta de KP/KD/Vel: "$line"');
  }
  return GainsStatus(
    kp: double.parse(m.group(1)!),
    kd: double.parse(m.group(2)!),
    speed: double.parse(m.group(3)!).round(),
  );
}

/// Parses a "<label> = <number>" line, e.g. "KP = 22.5" or "Velocidad = 180".
double parseAssignment(String line, String label) {
  final re = RegExp('${RegExp.escape(label)}\\s*=\\s*($_numPattern)');
  final m = re.firstMatch(line);
  if (m == null) {
    throw FormatException('No se pudo interpretar "$line" como asignación de $label');
  }
  return double.parse(m.group(1)!);
}

String? _find(List<String> lines, String prefix) {
  for (final line in lines) {
    if (line.trim().startsWith(prefix)) return line.trim();
  }
  return null;
}

/// Exact match on the text after the last ':', not substring containment —
/// "desactivado"/"desactivada" both contain "activ[oa]" as a substring, which
/// a naive `.contains('activo')` check would misreport as enabled.
bool _endsWithState(String? line, String activeWord) {
  if (line == null) return false;
  return line.split(':').last.trim() == activeWord;
}

PresetLoadResult parsePresetLoadBlock(int index, List<String> lines) {
  final trimmed = lines.map((l) => l.trim()).toList();
  if (trimmed.any((l) => l == 'Perfil invalido, usar 1..3')) {
    throw const RobotProtocolException('Perfil invalido, usar 1..3');
  }
  final kpLine = trimmed.firstWhere(
    (l) => l.startsWith('KP='),
    orElse: () => throw FormatException('Respuesta de carga de perfil incompleta: $trimmed'),
  );
  final m = RegExp('KP=($_numPattern) KD=($_numPattern) SPEED=($_numPattern)').firstMatch(kpLine);
  if (m == null) {
    throw FormatException('No se pudo interpretar la línea de perfil: "$kpLine"');
  }
  return PresetLoadResult(
    index: index,
    kp: double.parse(m.group(1)!),
    kd: double.parse(m.group(2)!),
    speed: double.parse(m.group(3)!).round(),
  );
}

PresetSaveResult parsePresetSaveLine(String line) {
  final trimmed = line.trim();
  if (trimmed == 'Perfil invalido, usar 1..3') {
    throw const RobotProtocolException('Perfil invalido, usar 1..3');
  }
  final m = RegExp(r'guardados en perfil (\d+)').firstMatch(trimmed);
  if (m == null) {
    throw FormatException('No se pudo interpretar confirmación de guardado: "$trimmed"');
  }
  return PresetSaveResult(index: int.parse(m.group(1)!));
}

List<SensorQuality> parseCalibrationQuality(List<String> lines) {
  final re = RegExp(r'S(\d+):\s*(-?\d+)\s*/\s*(-?\d+)\s*/\s*(-?\d+)');
  final result = <SensorQuality>[];
  for (final line in lines) {
    final m = re.firstMatch(line);
    if (m == null) continue;
    result.add(SensorQuality(
      index: int.parse(m.group(1)!),
      min: int.parse(m.group(2)!),
      max: int.parse(m.group(3)!),
      spread: int.parse(m.group(4)!),
      lowContrast: line.contains('BAJO CONTRASTE'),
    ));
  }
  return result;
}

PreflightReport parsePreflightBlock(List<String> lines) {
  final calibratedLine = _find(lines, 'Calibracion regleta:');
  final calibrated = calibratedLine?.contains('cargada') ?? false;
  final sensorQuality = calibrated ? parseCalibrationQuality(lines) : null;
  final lowContrastWarning = lines.any((l) => l.contains('Advertencia: uno o mas sensores'));

  final eepromLine = _find(lines, 'EEPROM ranura manual');
  final eepromSaved = eepromLine?.contains('OK, con datos guardados') ?? false;

  final presetLine = _find(lines, 'Perfil activo:');
  int? activePreset;
  if (presetLine != null) {
    final m = RegExp(r'Perfil activo:\s*(\d+)').firstMatch(presetLine);
    if (m != null) activePreset = int.parse(m.group(1)!);
  }

  final gainsLine = _find(lines, 'KP=');
  final gainsMatch =
      gainsLine != null ? RegExp('KP=($_numPattern) KD=($_numPattern) SPEED=($_numPattern)').firstMatch(gainsLine) : null;

  final softStartLine = _find(lines, 'Arranque suave');
  final softStart = _endsWithState(softStartLine, 'activo');

  final lineLossLine = _find(lines, 'Recuperacion perdida de linea');
  final lineLoss = _endsWithState(lineLossLine, 'activa');

  final timeoutLine = _find(lines, 'Timeout autotune');
  final timeoutMatch = timeoutLine != null ? RegExp(r'=\s*(\d+)\s*ms').firstMatch(timeoutLine) : null;

  final dtLine = _find(lines, 'loop dt idle');
  final dtMatch = dtLine != null ? RegExp('min=(\\d+)\\s*max=(\\d+)\\s*mean=($_numPattern)').firstMatch(dtLine) : null;

  return PreflightReport(
    calibrated: calibrated,
    sensorQuality: sensorQuality,
    lowContrastWarning: lowContrastWarning,
    eepromSaved: eepromSaved,
    activePreset: activePreset,
    kp: gainsMatch != null ? double.parse(gainsMatch.group(1)!) : 0,
    kd: gainsMatch != null ? double.parse(gainsMatch.group(2)!) : 0,
    speed: gainsMatch != null ? double.parse(gainsMatch.group(3)!).round() : 0,
    softStartEnabled: softStart,
    lineLossRecoveryEnabled: lineLoss,
    autotuneTimeoutMs: timeoutMatch != null ? int.parse(timeoutMatch.group(1)!) : 0,
    idleDtMinUs: dtMatch != null ? int.parse(dtMatch.group(1)!) : 0,
    idleDtMaxUs: dtMatch != null ? int.parse(dtMatch.group(2)!) : 0,
    idleDtMeanUs: dtMatch != null ? double.parse(dtMatch.group(3)!) : 0,
    raw: lines.join('\n'),
  );
}

RunSummary parseRunSummaryBlock(List<String> lines) {
  // [key] already includes the trailing '=' (e.g. 'muestras='), so this just
  // reads the digits immediately following it — no second '=' in the regex.
  int intAfter(String key) {
    final line = _find(lines, key);
    if (line == null) return 0;
    final m = RegExp(r'^(\d+)').firstMatch(line.substring(key.length));
    return m != null ? int.parse(m.group(1)!) : 0;
  }

  final samples = intAfter('muestras=');

  final peakLine = _find(lines, 'peak') ?? '';
  final peak = int.tryParse(RegExp(r'=\s*(\d+)').firstMatch(peakLine)?.group(1) ?? '') ?? 0;

  final meanLine = _find(lines, 'mean') ?? '';
  final mean = double.tryParse(RegExp('=\\s*($_numPattern)').firstMatch(meanLine)?.group(1) ?? '') ?? 0;

  // The "(>=340)" threshold in this line has its own '=' before the real
  // assignment, so anchor on the ')' that precedes "= <n>" rather than
  // matching the first '=' in the line.
  final satLine = _find(lines, 'ciclos en saturacion') ?? '';
  final saturation = int.tryParse(RegExp(r'\)\s*=\s*(\d+)').firstMatch(satLine)?.group(1) ?? '') ?? 0;

  final lossLine = _find(lines, 'perdida de linea') ?? '';
  final lossMatch = RegExp(r'eventos=(\d+)\s*ciclos=(\d+)').firstMatch(lossLine);

  final dtLine = _find(lines, 'loop dt') ?? '';
  final dtMatch = RegExp('min=(\\d+)\\s*max=(\\d+)\\s*mean=($_numPattern)').firstMatch(dtLine);

  final gainsLine = _find(lines, 'KP=') ?? '';
  final gainsMatch = RegExp('KP=($_numPattern) KD=($_numPattern) SPEED=($_numPattern)').firstMatch(gainsLine);

  final presetLine = _find(lines, 'perfil activo') ?? '';
  int? activePreset;
  if (!presetLine.contains('ninguno')) {
    final m = RegExp(r'=\s*(\d+)').firstMatch(presetLine);
    if (m != null) activePreset = int.parse(m.group(1)!);
  }

  return RunSummary(
    samples: samples,
    peakPosition: peak,
    meanAbsPosition: mean,
    saturationCycles: saturation,
    lineLossEvents: lossMatch != null ? int.parse(lossMatch.group(1)!) : 0,
    lineLossCycles: lossMatch != null ? int.parse(lossMatch.group(2)!) : 0,
    dtMinUs: dtMatch != null ? int.parse(dtMatch.group(1)!) : 0,
    dtMaxUs: dtMatch != null ? int.parse(dtMatch.group(2)!) : 0,
    dtMeanUs: dtMatch != null ? double.parse(dtMatch.group(3)!) : 0,
    kp: gainsMatch != null ? double.parse(gainsMatch.group(1)!) : 0,
    kd: gainsMatch != null ? double.parse(gainsMatch.group(2)!) : 0,
    speed: gainsMatch != null ? double.parse(gainsMatch.group(3)!).round() : 0,
    activePreset: activePreset,
    raw: lines.join('\n'),
  );
}

TelemetryDump parseTelemetryDumpBlock(List<String> lines) {
  final gainsLine = _find(lines, 'KP=') ?? '';
  final gainsMatch = RegExp('KP=($_numPattern) KD=($_numPattern) SPEED=($_numPattern)').firstMatch(gainsLine);

  final dtLine = _find(lines, 'loop_dt_us') ?? '';
  final dtMatch = RegExp('min=(\\d+)\\s*max=(\\d+)\\s*mean=($_numPattern)').firstMatch(dtLine);

  final metaLine = _find(lines, 'samples=') ?? '';
  final metaMatch = RegExp(r'samples=(\d+)\s*decimation=(\d+)').firstMatch(metaLine);

  final rows = <TelemetrySample>[];
  var inCsv = false;
  for (final raw in lines) {
    final line = raw.trim();
    if (line == 'dt_us,posicion,corr') {
      inCsv = true;
      continue;
    }
    if (!inCsv) continue;
    if (line == '--- END ---') break;
    final parts = line.split(',');
    if (parts.length != 3) continue;
    final dt = int.tryParse(parts[0]);
    final pos = int.tryParse(parts[1]);
    final corr = int.tryParse(parts[2]);
    if (dt == null || pos == null || corr == null) continue;
    rows.add(TelemetrySample(dtUs: dt, posicion: pos, corr: corr));
  }

  return TelemetryDump(
    kp: gainsMatch != null ? double.parse(gainsMatch.group(1)!) : 0,
    kd: gainsMatch != null ? double.parse(gainsMatch.group(2)!) : 0,
    speed: gainsMatch != null ? double.parse(gainsMatch.group(3)!).round() : 0,
    dtMinUs: dtMatch != null ? int.parse(dtMatch.group(1)!) : 0,
    dtMaxUs: dtMatch != null ? int.parse(dtMatch.group(2)!) : 0,
    dtMeanUs: dtMatch != null ? double.parse(dtMatch.group(3)!) : 0,
    samples: metaMatch != null ? int.parse(metaMatch.group(1)!) : rows.length,
    decimation: metaMatch != null ? int.parse(metaMatch.group(2)!) : 1,
    rows: rows,
  );
}

AutotuneResult parseAutotuneBlock(List<String> lines) {
  final trimmed = lines.map((l) => l.trim()).toList();
  final raw = lines.join('\n');

  final failureLine = trimmed.firstWhere(
    (l) => l.startsWith('Autotune abortado') || l.startsWith('Autotune failed'),
    orElse: () => '',
  );
  if (failureLine.isNotEmpty) {
    return AutotuneResult.failure(reason: failureLine, raw: raw);
  }

  double? afterEquals(String key) {
    final line = trimmed.firstWhere((l) => l.startsWith(key), orElse: () => '');
    if (line.isEmpty) return null;
    final m = RegExp('=\\s*($_numPattern)').firstMatch(line);
    return m != null ? double.parse(m.group(1)!) : null;
  }

  return AutotuneResult.success(
    raw: raw,
    a: afterEquals('a (PV amp)'),
    pu: afterEquals('Pu (s)'),
    ku: afterEquals('Ku'),
    suggestedKp: afterEquals('Suggested KP'),
    suggestedKd: afterEquals('Suggested KD'),
    installedKp: afterEquals('Installed KP'),
    installedKd: afterEquals('Installed KD'),
  );
}

GainsStatus parseManualSlotBlock(List<String> lines) {
  final trimmed = lines.map((l) => l.trim()).toList();
  String find(String prefix) => trimmed.firstWhere((l) => l.startsWith(prefix), orElse: () => '');
  return GainsStatus(
    kp: parseAssignment(find('KP ='), 'KP'),
    kd: parseAssignment(find('KD ='), 'KD'),
    speed: parseAssignment(find('SPEED ='), 'SPEED').round(),
  );
}
