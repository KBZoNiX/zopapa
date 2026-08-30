import 'package:flutter/material.dart';

import '../../protocol/models.dart';

class RunSummaryCard extends StatelessWidget {
  const RunSummaryCard({super.key, required this.summary});

  final RunSummary summary;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Resumen de la última corrida', style: theme.textTheme.titleMedium),
            const SizedBox(height: 8),
            Text('Muestras: ${summary.samples}'),
            Text('Error pico / medio: ${summary.peakPosition} / ${summary.meanAbsPosition.toStringAsFixed(2)}'),
            Text('Ciclos en saturación: ${summary.saturationCycles}'),
            Text('Pérdida de línea: ${summary.lineLossEvents} eventos, ${summary.lineLossCycles} ciclos'),
            Text('Loop dt (µs): min=${summary.dtMinUs} max=${summary.dtMaxUs} mean=${summary.dtMeanUs.toStringAsFixed(1)}'),
            Text('KP=${summary.kp.toStringAsFixed(2)} KD=${summary.kd.toStringAsFixed(2)} Velocidad=${summary.speed}'),
            Text('Perfil activo: ${summary.activePreset?.toString() ?? 'ninguno (manual)'}'),
          ],
        ),
      ),
    );
  }
}
