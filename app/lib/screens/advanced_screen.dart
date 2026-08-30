import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../protocol/exceptions.dart';
import '../protocol/models.dart';
import '../protocol/robot_client.dart';
import '../state/robot_state.dart';
import 'widgets/confirm_action_dialog.dart';
import 'widgets/telemetry_table.dart';

/// Bench-test/practice tools — not for match play (see §7.4 in
/// docs/Reglamento-Carreras.md). Reached from Home via an explicit menu item.
class AdvancedScreen extends StatefulWidget {
  const AdvancedScreen({super.key});

  @override
  State<AdvancedScreen> createState() => _AdvancedScreenState();
}

class _AdvancedScreenState extends State<AdvancedScreen> {
  bool _busy = false;
  TelemetryDump? _lastDump;
  final _rawController = TextEditingController();
  final _rawLog = <String>[];

  @override
  void initState() {
    super.initState();
    context.read<RobotState>().client.rawLines.listen((line) {
      if (!mounted) return;
      setState(() {
        _rawLog.add(line);
        if (_rawLog.length > 200) _rawLog.removeAt(0);
      });
    });
  }

  @override
  void dispose() {
    _rawController.dispose();
    super.dispose();
  }

  Future<void> _run(Future<void> Function() action) async {
    if (_busy) return;
    setState(() => _busy = true);
    try {
      await action();
    } catch (e) {
      if (!mounted) return;
      final message = e is RobotProtocolException ? e.message : e.toString();
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message)));
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final state = context.watch<RobotState>();
    final client = state.client;
    final locked = _busy || state.isRunning;
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(title: const Text('Avanzado / práctica')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(color: theme.colorScheme.errorContainer, borderRadius: BorderRadius.circular(8)),
            child: const Text(
              'Estas herramientas son para práctica y ajuste en el banco. El reglamento de la '
              'competencia (§7.4) no permite usarlas durante un encuentro oficial.',
            ),
          ),
          const SizedBox(height: 16),
          Text('Pruebas con riesgo', style: theme.textTheme.titleMedium),
          const SizedBox(height: 8),
          Row(
            children: [
              Expanded(
                child: OutlinedButton(
                  onPressed: locked
                      ? null
                      : () => confirmAndRun(
                            context,
                            title: 'Probar regleta (Q)',
                            message: 'Esta prueba corre en bucle y solo se puede detener con el pulsador '
                                'físico del robot — no hay forma de pararla por Bluetooth. Asegurate de '
                                'tener acceso al robot antes de continuar.',
                            action: () => _run(() => client.sendRaw('Q')),
                          ),
                  child: const Text('Probar regleta (Q)'),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: OutlinedButton(
                  onPressed: locked
                      ? null
                      : () => confirmAndRun(
                            context,
                            title: 'Recto / lazo abierto (O)',
                            message: 'El robot avanza sin leer la regleta y sin frenado automático por '
                                'pérdida de línea. Solo "Detener" lo para.',
                            action: () => _run(() => state.driveStraightOpenLoop()),
                          ),
                  child: const Text('Recto (O)'),
                ),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Text('Telemetría / ajuste de AutoTune', style: theme.textTheme.titleMedium),
          const SizedBox(height: 8),
          _NumericCommandRow(
            label: 'Decimación (N)',
            enabled: !locked,
            onSubmit: (v) => _run(() => client.setDecimation(v.round())),
          ),
          _NumericCommandRow(
            label: 'Amplitud relé fija (A, 0 = proporcional)',
            enabled: !locked,
            onSubmit: (v) => _run(() => client.setRelayAmplitude(v.round())),
          ),
          _NumericCommandRow(
            label: 'Timeout AutoTune ms (M)',
            enabled: !locked,
            onSubmit: (v) => _run(() => client.setAutotuneTimeout(v.round())),
          ),
          const SizedBox(height: 8),
          OutlinedButton.icon(
            icon: const Icon(Icons.download),
            label: const Text('Volcar telemetría (Z)'),
            onPressed: locked
                ? null
                : () => _run(() async {
                      final dump = await client.dumpTelemetry();
                      setState(() => _lastDump = dump);
                    }),
          ),
          if (_lastDump != null) ...[
            const SizedBox(height: 8),
            TelemetryTable(dump: _lastDump!),
          ],
          const SizedBox(height: 16),
          Text('Ranura manual EEPROM (L/W)', style: theme.textTheme.titleMedium),
          const SizedBox(height: 8),
          Row(
            children: [
              Expanded(
                child: OutlinedButton(
                  onPressed: locked ? null : () => _run(() => client.loadManualSlot()),
                  child: const Text('Cargar (L)'),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: OutlinedButton(
                  onPressed: locked ? null : () => _run(() => client.saveManualSlot()),
                  child: const Text('Guardar (W)'),
                ),
              ),
            ],
          ),
          const SizedBox(height: 16),
          Text('Terminal cruda', style: theme.textTheme.titleMedium),
          const SizedBox(height: 8),
          Container(
            height: 160,
            padding: const EdgeInsets.all(8),
            decoration: BoxDecoration(
              color: theme.colorScheme.surfaceContainerHighest,
              borderRadius: BorderRadius.circular(8),
            ),
            child: ListView.builder(
              reverse: true,
              itemCount: _rawLog.length,
              itemBuilder: (ctx, i) => Text(
                _rawLog[_rawLog.length - 1 - i],
                style: theme.textTheme.bodySmall?.copyWith(fontFamily: 'monospace'),
              ),
            ),
          ),
          const SizedBox(height: 8),
          Row(
            children: [
              Expanded(
                child: TextField(
                  controller: _rawController,
                  decoration: const InputDecoration(labelText: 'Comando', isDense: true),
                  onSubmitted: (text) => _sendRawCommand(client, text),
                ),
              ),
              const SizedBox(width: 8),
              FilledButton(
                onPressed: () => _sendRawCommand(client, _rawController.text),
                child: const Text('Enviar'),
              ),
            ],
          ),
        ],
      ),
    );
  }

  void _sendRawCommand(RobotClient client, String text) {
    final command = text.trim();
    if (command.isEmpty) return;
    _rawController.clear();
    client.sendRaw(command);
  }
}

class _NumericCommandRow extends StatefulWidget {
  const _NumericCommandRow({required this.label, required this.enabled, required this.onSubmit});

  final String label;
  final bool enabled;
  final void Function(double value) onSubmit;

  @override
  State<_NumericCommandRow> createState() => _NumericCommandRowState();
}

class _NumericCommandRowState extends State<_NumericCommandRow> {
  final _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          Expanded(
            child: TextField(
              controller: _controller,
              enabled: widget.enabled,
              keyboardType: TextInputType.number,
              decoration: InputDecoration(labelText: widget.label, isDense: true),
            ),
          ),
          const SizedBox(width: 8),
          FilledButton(
            onPressed: widget.enabled
                ? () {
                    final value = double.tryParse(_controller.text.trim());
                    if (value != null) widget.onSubmit(value);
                  }
                : null,
            child: const Text('Enviar'),
          ),
        ],
      ),
    );
  }
}
