class GainsStatus {
  const GainsStatus({required this.kp, required this.kd, required this.speed});

  final double kp;
  final double kd;
  final int speed;

  GainsStatus copyWith({double? kp, double? kd, int? speed}) => GainsStatus(
        kp: kp ?? this.kp,
        kd: kd ?? this.kd,
        speed: speed ?? this.speed,
      );
}

class PresetLoadResult {
  const PresetLoadResult({required this.index, required this.kp, required this.kd, required this.speed});

  final int index;
  final double kp;
  final double kd;
  final int speed;
}

class PresetSaveResult {
  const PresetSaveResult({required this.index});

  final int index;
}

class SensorQuality {
  const SensorQuality({
    required this.index,
    required this.min,
    required this.max,
    required this.spread,
    required this.lowContrast,
  });

  final int index;
  final int min;
  final int max;
  final int spread;
  final bool lowContrast;
}

class PreflightReport {
  const PreflightReport({
    required this.calibrated,
    required this.sensorQuality,
    required this.lowContrastWarning,
    required this.eepromSaved,
    required this.activePreset,
    required this.kp,
    required this.kd,
    required this.speed,
    required this.softStartEnabled,
    required this.lineLossRecoveryEnabled,
    required this.autotuneTimeoutMs,
    required this.idleDtMinUs,
    required this.idleDtMaxUs,
    required this.idleDtMeanUs,
    required this.raw,
  });

  final bool calibrated;
  final List<SensorQuality>? sensorQuality;
  final bool lowContrastWarning;
  final bool eepromSaved;
  final int? activePreset;
  final double kp;
  final double kd;
  final int speed;
  final bool softStartEnabled;
  final bool lineLossRecoveryEnabled;
  final int autotuneTimeoutMs;
  final int idleDtMinUs;
  final int idleDtMaxUs;
  final double idleDtMeanUs;
  final String raw;
}

class RunSummary {
  const RunSummary({
    required this.samples,
    required this.peakPosition,
    required this.meanAbsPosition,
    required this.saturationCycles,
    required this.lineLossEvents,
    required this.lineLossCycles,
    required this.dtMinUs,
    required this.dtMaxUs,
    required this.dtMeanUs,
    required this.kp,
    required this.kd,
    required this.speed,
    required this.activePreset,
    required this.raw,
  });

  final int samples;
  final int peakPosition;
  final double meanAbsPosition;
  final int saturationCycles;
  final int lineLossEvents;
  final int lineLossCycles;
  final int dtMinUs;
  final int dtMaxUs;
  final double dtMeanUs;
  final double kp;
  final double kd;
  final int speed;
  final int? activePreset;
  final String raw;
}

class TelemetrySample {
  const TelemetrySample({required this.dtUs, required this.posicion, required this.corr});

  final int dtUs;
  final int posicion;
  final int corr;
}

class TelemetryDump {
  const TelemetryDump({
    required this.kp,
    required this.kd,
    required this.speed,
    required this.dtMinUs,
    required this.dtMaxUs,
    required this.dtMeanUs,
    required this.samples,
    required this.decimation,
    required this.rows,
  });

  final double kp;
  final double kd;
  final int speed;
  final int dtMinUs;
  final int dtMaxUs;
  final double dtMeanUs;
  final int samples;
  final int decimation;
  final List<TelemetrySample> rows;
}

class AutotuneResult {
  const AutotuneResult._({
    required this.ok,
    required this.raw,
    this.failureReason,
    this.a,
    this.pu,
    this.ku,
    this.suggestedKp,
    this.suggestedKd,
    this.installedKp,
    this.installedKd,
  });

  factory AutotuneResult.success({
    required String raw,
    double? a,
    double? pu,
    double? ku,
    double? suggestedKp,
    double? suggestedKd,
    double? installedKp,
    double? installedKd,
  }) =>
      AutotuneResult._(
        ok: true,
        raw: raw,
        a: a,
        pu: pu,
        ku: ku,
        suggestedKp: suggestedKp,
        suggestedKd: suggestedKd,
        installedKp: installedKp,
        installedKd: installedKd,
      );

  factory AutotuneResult.failure({required String reason, required String raw}) =>
      AutotuneResult._(ok: false, raw: raw, failureReason: reason);

  final bool ok;
  final String raw;
  final String? failureReason;
  final double? a;
  final double? pu;
  final double? ku;
  final double? suggestedKp;
  final double? suggestedKd;
  final double? installedKp;
  final double? installedKd;
}
