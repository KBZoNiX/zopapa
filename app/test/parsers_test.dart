import 'package:flutter_test/flutter_test.dart';
import 'package:zopapa_control/protocol/exceptions.dart';
import 'package:zopapa_control/protocol/parsers.dart';

void main() {
  group('parseGainsLine', () {
    test('parses an R response', () {
      final gains = parseGainsLine('KP: 2.5 | KD: 10.0 | Vel: 200');
      expect(gains.kp, 2.5);
      expect(gains.kd, 10.0);
      expect(gains.speed, 200);
    });
  });

  group('parseAssignment', () {
    test('parses a KP assignment', () {
      expect(parseAssignment('KP = 22.5', 'KP'), 22.5);
    });

    test('parses a Velocidad assignment', () {
      expect(parseAssignment('Velocidad = 180', 'Velocidad'), 180);
    });
  });

  group('parsePresetLoadBlock', () {
    test('parses a valid preset load', () {
      final result = parsePresetLoadBlock(2, ['Perfil 2 cargado:', 'KP=2.0000 KD=10.0000 SPEED=200']);
      expect(result.index, 2);
      expect(result.kp, 2.0);
      expect(result.kd, 10.0);
      expect(result.speed, 200);
    });

    test('throws on an invalid preset index', () {
      expect(
        () => parsePresetLoadBlock(9, ['Perfil invalido, usar 1..3']),
        throwsA(isA<RobotProtocolException>()),
      );
    });
  });

  group('parsePresetSaveLine', () {
    test('parses a successful save', () {
      expect(parsePresetSaveLine('KP/KD/SPEED actuales guardados en perfil 3').index, 3);
    });

    test('throws on an invalid preset index', () {
      expect(
        () => parsePresetSaveLine('Perfil invalido, usar 1..3'),
        throwsA(isA<RobotProtocolException>()),
      );
    });
  });

  group('parseCalibrationQuality', () {
    test('parses sensor lines and flags low contrast', () {
      final result = parseCalibrationQuality([
        'Calidad de calibracion (min / max / spread):',
        '  S0: 10 / 200 / 190',
        '  S1: 80 / 100 / 20  <-- BAJO CONTRASTE',
      ]);
      expect(result.length, 2);
      expect(result[0].spread, 190);
      expect(result[0].lowContrast, isFalse);
      expect(result[1].lowContrast, isTrue);
    });
  });

  group('parsePreflightBlock', () {
    test('parses a full preflight report', () {
      final report = parsePreflightBlock([
        '--- PREFLIGHT ---',
        'Calibracion regleta: cargada',
        'Calidad de calibracion (min / max / spread):',
        '  S0: 10 / 200 / 190',
        'EEPROM ranura manual (L/W): OK, con datos guardados',
        'Perfil activo: 2',
        'KP=2.0000 KD=10.0000 SPEED=200',
        'Gain scheduling: no implementado en este build (tournament-stopgap).',
        'Arranque suave (T-S8): desactivado',
        'Recuperacion perdida de linea (T-S7): desactivada',
        'Timeout autotune = 8000 ms',
        'loop dt idle (us) sobre 59 muestras: min=1900 max=2100 mean=1980.50',
        '--- FIN PREFLIGHT ---',
      ]);
      expect(report.calibrated, isTrue);
      expect(report.sensorQuality?.length, 1);
      expect(report.eepromSaved, isTrue);
      expect(report.activePreset, 2);
      expect(report.kp, 2.0);
      expect(report.speed, 200);
      expect(report.softStartEnabled, isFalse);
      expect(report.lineLossRecoveryEnabled, isFalse);
      expect(report.autotuneTimeoutMs, 8000);
      expect(report.idleDtMinUs, 1900);
      expect(report.idleDtMeanUs, 1980.5);
    });

    test('reports SIN CALIBRAR with no sensor quality block', () {
      final report = parsePreflightBlock([
        '--- PREFLIGHT ---',
        'Calibracion regleta: SIN CALIBRAR',
        'EEPROM ranura manual (L/W): sin guardar (valores por defecto)',
        'Perfil activo: ninguno (valores manuales)',
        'KP=1.1500 KD=5.0000 SPEED=150',
        'Arranque suave (T-S8): desactivado',
        'Recuperacion perdida de linea (T-S7): desactivada',
        'Timeout autotune = 8000 ms',
        'loop dt idle (us) sobre 59 muestras: min=1900 max=2100 mean=1980.50',
        '--- FIN PREFLIGHT ---',
      ]);
      expect(report.calibrated, isFalse);
      expect(report.sensorQuality, isNull);
      expect(report.eepromSaved, isFalse);
      expect(report.activePreset, isNull);
    });
  });

  group('parseRunSummaryBlock', () {
    test('parses a full run summary block', () {
      final summary = parseRunSummaryBlock([
        '--- RESUMEN DE CORRIDA ---',
        'muestras=1200',
        'peak |posicion| = 310',
        'mean |posicion| = 42.50',
        'ciclos en saturacion (>=340) = 3',
        'perdida de linea: eventos=1 ciclos=15',
        'loop dt (us) min=1800 max=2200 mean=1950.30',
        'KP=2.0000 KD=10.0000 SPEED=200',
        'perfil activo = 2',
        '--- FIN RESUMEN ---',
      ]);
      expect(summary.samples, 1200);
      expect(summary.peakPosition, 310);
      expect(summary.meanAbsPosition, 42.5);
      expect(summary.saturationCycles, 3);
      expect(summary.lineLossEvents, 1);
      expect(summary.lineLossCycles, 15);
      expect(summary.dtMinUs, 1800);
      expect(summary.dtMaxUs, 2200);
      expect(summary.activePreset, 2);
    });

    test('treats "ninguno" as no active preset', () {
      final summary = parseRunSummaryBlock([
        '--- RESUMEN DE CORRIDA ---',
        'muestras=10',
        'peak |posicion| = 5',
        'mean |posicion| = 1.00',
        'ciclos en saturacion (>=340) = 0',
        'perdida de linea: eventos=0 ciclos=0',
        'loop dt (us) min=1900 max=2000 mean=1950.00',
        'KP=1.1500 KD=5.0000 SPEED=150',
        'perfil activo = ninguno (manual)',
        '--- FIN RESUMEN ---',
      ]);
      expect(summary.activePreset, isNull);
    });
  });

  group('parseTelemetryDumpBlock', () {
    test('parses header and CSV rows', () {
      final dump = parseTelemetryDumpBlock([
        '--- TELEMETRY DUMP ---',
        'KP=2.0000 KD=10.0000 SPEED=200',
        'loop_dt_us min=1800 max=2200 mean=1950.30',
        'samples=2 decimation=1',
        'dt_us,posicion,corr',
        '1950,10,-25',
        '1960,-5,12',
        '--- END ---',
      ]);
      expect(dump.samples, 2);
      expect(dump.decimation, 1);
      expect(dump.rows.length, 2);
      expect(dump.rows[0].dtUs, 1950);
      expect(dump.rows[1].corr, 12);
    });
  });

  group('parseAutotuneBlock', () {
    test('parses a successful run', () {
      final result = parseAutotuneBlock([
        'Iniciando AutoTune...',
        '>>> AUTOTUNE START (relay method)',
        'Relay amplitude (d) = 50',
        'Peak! 120',
        '=== AUTOTUNE RESULTS ===',
        'a (PV amp) = 3.5000',
        'Pu (s) = 0.4200',
        'Ku = 2.1000',
        'Suggested KP = 1.2600',
        'Suggested KD = 0.0530',
        'Installed KP = 1.2600',
        'Installed KD = 0.0530',
        '>>> AUTOTUNE END',
        'AutoTune Finalizado.',
      ]);
      expect(result.ok, isTrue);
      expect(result.installedKp, 1.26);
      expect(result.installedKd, 0.053);
    });

    test('parses an edge-abort failure', () {
      final result = parseAutotuneBlock([
        'Iniciando AutoTune...',
        '>>> AUTOTUNE START (relay method)',
        'Autotune abortado: posición cerca del borde de la regleta.',
        'AutoTune Finalizado.',
      ]);
      expect(result.ok, isFalse);
      expect(result.failureReason, contains('abortado'));
    });
  });

  group('parseManualSlotBlock', () {
    test('parses the L/W multi-line block', () {
      final gains = parseManualSlotBlock([
        'Constantes PID cargadas desde EEPROM:',
        'KP = 2.5000',
        'KD = 10.0000',
        'SPEED = 200',
      ]);
      expect(gains.kp, 2.5);
      expect(gains.kd, 10.0);
      expect(gains.speed, 200);
    });
  });
}
