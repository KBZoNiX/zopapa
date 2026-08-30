import 'package:flutter/material.dart';

import '../../protocol/models.dart';

class TelemetryTable extends StatelessWidget {
  const TelemetryTable({super.key, required this.dump});

  final TelemetryDump dump;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'KP=${dump.kp.toStringAsFixed(2)} KD=${dump.kd.toStringAsFixed(2)} Velocidad=${dump.speed} · '
              'dt(µs) min=${dump.dtMinUs} max=${dump.dtMaxUs} mean=${dump.dtMeanUs.toStringAsFixed(1)}',
              style: Theme.of(context).textTheme.bodySmall,
            ),
            Text('Muestras: ${dump.samples} · Decimación: ${dump.decimation}',
                style: Theme.of(context).textTheme.bodySmall),
            const SizedBox(height: 8),
            SizedBox(
              height: 240,
              child: SingleChildScrollView(
                child: DataTable(
                  columns: const [
                    DataColumn(label: Text('#')),
                    DataColumn(label: Text('dt_us')),
                    DataColumn(label: Text('posicion')),
                    DataColumn(label: Text('corr')),
                  ],
                  rows: [
                    for (var i = 0; i < dump.rows.length; i++)
                      DataRow(cells: [
                        DataCell(Text('$i')),
                        DataCell(Text('${dump.rows[i].dtUs}')),
                        DataCell(Text('${dump.rows[i].posicion}')),
                        DataCell(Text('${dump.rows[i].corr}')),
                      ]),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
