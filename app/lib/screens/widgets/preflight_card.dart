import 'package:flutter/material.dart';

import '../../protocol/models.dart';

class PreflightCard extends StatelessWidget {
  const PreflightCard({super.key, required this.report});

  final PreflightReport report;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final okColor = theme.colorScheme.primary;
    final warnColor = theme.colorScheme.error;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Chequeo previo', style: theme.textTheme.titleMedium),
            const SizedBox(height: 8),
            _Row('Calibración', report.calibrated ? 'Cargada' : 'SIN CALIBRAR',
                ok: report.calibrated, okColor: okColor, warnColor: warnColor),
            if (report.lowContrastWarning)
              Padding(
                padding: const EdgeInsets.only(top: 4, bottom: 4),
                child: Text(
                  'Uno o más sensores con poco contraste. Revisar suciedad/alineación.',
                  style: TextStyle(color: warnColor),
                ),
              ),
            _Row('EEPROM manual (L/W)', report.eepromSaved ? 'Con datos guardados' : 'Sin guardar (por defecto)',
                ok: report.eepromSaved, okColor: okColor, warnColor: warnColor),
            _Row('Perfil activo', report.activePreset?.toString() ?? 'Ninguno (manual)',
                ok: true, okColor: okColor, warnColor: warnColor),
            _Row(
              'KP / KD / Velocidad',
              '${report.kp.toStringAsFixed(2)} / ${report.kd.toStringAsFixed(2)} / ${report.speed}',
              ok: true,
              okColor: okColor,
              warnColor: warnColor,
            ),
            _Row('Arranque suave (T-S8)', report.softStartEnabled ? 'Activo' : 'Desactivado',
                ok: true, okColor: okColor, warnColor: warnColor),
            _Row(
              'Recuperación pérdida de línea (T-S7)',
              report.lineLossRecoveryEnabled ? 'Activa' : 'Desactivada',
              ok: true,
              okColor: okColor,
              warnColor: warnColor,
            ),
            _Row('Timeout AutoTune', '${report.autotuneTimeoutMs} ms',
                ok: true, okColor: okColor, warnColor: warnColor),
            _Row(
              'Loop dt idle (µs)',
              'min=${report.idleDtMinUs} max=${report.idleDtMaxUs} mean=${report.idleDtMeanUs.toStringAsFixed(1)}',
              ok: true,
              okColor: okColor,
              warnColor: warnColor,
            ),
          ],
        ),
      ),
    );
  }
}

class _Row extends StatelessWidget {
  const _Row(this.label, this.value, {required this.ok, required this.okColor, required this.warnColor});

  final String label;
  final String value;
  final bool ok;
  final Color okColor;
  final Color warnColor;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Expanded(flex: 3, child: Text(label)),
          Expanded(
            flex: 3,
            child: Text(value, style: TextStyle(color: ok ? okColor : warnColor, fontWeight: FontWeight.w600)),
          ),
        ],
      ),
    );
  }
}
