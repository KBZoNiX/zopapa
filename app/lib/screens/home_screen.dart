import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../protocol/exceptions.dart';
import '../protocol/models.dart';
import '../state/robot_state.dart';
import 'advanced_screen.dart';
import 'widgets/confirm_action_dialog.dart';
import 'widgets/preflight_card.dart';
import 'widgets/preset_card.dart';
import 'widgets/run_summary_card.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  bool _busy = false;

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
    final locked = _busy || state.isRunning;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Zopapa Control'),
        actions: [
          IconButton(
            icon: const Icon(Icons.build),
            tooltip: 'Avanzado / práctica',
            onPressed: state.isRunning
                ? null
                : () => Navigator.of(context).push(MaterialPageRoute(builder: (_) => const AdvancedScreen())),
          ),
          IconButton(
            icon: const Icon(Icons.bluetooth_disabled),
            tooltip: 'Desconectar',
            onPressed: () => _run(() => state.disconnect()),
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          SizedBox(
            height: 72,
            child: state.isRunning
                ? FilledButton.icon(
                    style: FilledButton.styleFrom(backgroundColor: Theme.of(context).colorScheme.error),
                    icon: const Icon(Icons.stop),
                    label: const Text('DETENER', style: TextStyle(fontSize: 20)),
                    onPressed: () => _run(() => state.stop()),
                  )
                : FilledButton.icon(
                    icon: const Icon(Icons.play_arrow),
                    label: const Text('ARRANCAR', style: TextStyle(fontSize: 20)),
                    onPressed: _busy ? null : () => _run(() => state.start()),
                  ),
          ),
          const SizedBox(height: 16),
          Text('Perfiles', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 8),
          Row(
            children: [
              for (final i in [1, 2, 3])
                Expanded(
                  child: Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 4),
                    child: PresetCard(
                      index: i,
                      active: state.activePreset == i,
                      enabled: !locked,
                      onLoad: () => _run(() => state.loadPreset(i)),
                      onSave: () => _run(() => state.savePreset(i)),
                    ),
                  ),
                ),
            ],
          ),
          const SizedBox(height: 16),
          _ManualGainsCard(state: state, enabled: !locked, run: _run),
          const SizedBox(height: 16),
          Row(
            children: [
              Expanded(
                child: OutlinedButton.icon(
                  icon: const Icon(Icons.fact_check),
                  label: const Text('Chequeo previo'),
                  onPressed: locked ? null : () => _run(() => state.runPreflight()),
                ),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: OutlinedButton.icon(
                  icon: const Icon(Icons.tune),
                  label: const Text('Calibrar'),
                  onPressed: locked
                      ? null
                      : () => confirmAndRun(
                            context,
                            title: 'Recalibrar regleta',
                            message: 'Barrido de 3 segundos. Pasá la regleta sobre la línea cuando empiece.',
                            action: () => _run(() => state.calibrate()),
                          ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 8),
          OutlinedButton.icon(
            icon: const Icon(Icons.speed),
            label: const Text('AutoTune'),
            onPressed: locked
                ? null
                : () => confirmAndRun(
                      context,
                      title: 'AutoTune',
                      message: 'El robot va a corregir de un lado a otro brevemente. '
                          'Hacelo sobre una guía recta o con las ruedas levantadas la primera vez.',
                      action: () => _run(() => state.autotune()),
                    ),
          ),
          if (state.lastPreflight != null) ...[
            const SizedBox(height: 16),
            PreflightCard(report: state.lastPreflight!),
          ],
          if (state.lastRunSummary != null) ...[
            const SizedBox(height: 16),
            RunSummaryCard(summary: state.lastRunSummary!),
          ],
        ],
      ),
    );
  }
}

class _ManualGainsCard extends StatefulWidget {
  const _ManualGainsCard({required this.state, required this.enabled, required this.run});

  final RobotState state;
  final bool enabled;
  final Future<void> Function(Future<void> Function()) run;

  @override
  State<_ManualGainsCard> createState() => _ManualGainsCardState();
}

class _ManualGainsCardState extends State<_ManualGainsCard> {
  final _kpController = TextEditingController();
  final _kdController = TextEditingController();
  final _speedController = TextEditingController();

  // Tracks the last GainsStatus we've synced the fields to, by identity —
  // RobotState only reassigns lastGains when a command actually changes it
  // (preset load, refresh, autotune, run summary...), so identity is stable
  // across unrelated rebuilds (e.g. isRunning toggling alone).
  GainsStatus? _syncedGains;

  @override
  void dispose() {
    _kpController.dispose();
    _kdController.dispose();
    _speedController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final gains = widget.state.lastGains;
    if (!identical(gains, _syncedGains)) {
      _syncedGains = gains;
      // An external update (preset load, refresh, autotune, run summary)
      // landed — show its values as pre-filled text. hintText alone isn't
      // enough: with both labelText and hintText set, Material's TextField
      // only reveals the hint once the label floats (on focus or non-empty
      // text), so an unfocused field would keep showing just "KP" instead
      // of the loaded value.
      _kpController.text = gains?.kp.toStringAsFixed(2) ?? '';
      _kdController.text = gains?.kd.toStringAsFixed(2) ?? '';
      _speedController.text = gains?.speed.toString() ?? '';
    }
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Text('Ganancias manuales', style: Theme.of(context).textTheme.titleMedium),
                const Spacer(),
                Text(
                  widget.state.activePreset != null ? 'Perfil ${widget.state.activePreset}' : 'Manual',
                  style: Theme.of(context).textTheme.labelMedium,
                ),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              children: [
                Expanded(
                  child: _GainField(
                    label: 'KP',
                    controller: _kpController,
                    enabled: widget.enabled,
                    onSubmit: (v) => widget.run(() => widget.state.setKp(v)),
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: _GainField(
                    label: 'KD',
                    controller: _kdController,
                    enabled: widget.enabled,
                    onSubmit: (v) => widget.run(() => widget.state.setKd(v)),
                  ),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: _GainField(
                    label: 'Velocidad',
                    controller: _speedController,
                    enabled: widget.enabled,
                    onSubmit: (v) => widget.run(() => widget.state.setSpeed(v.round())),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _GainField extends StatelessWidget {
  const _GainField({
    required this.label,
    required this.controller,
    required this.enabled,
    required this.onSubmit,
  });

  final String label;
  final TextEditingController controller;
  final bool enabled;
  final void Function(double value) onSubmit;

  @override
  Widget build(BuildContext context) {
    return TextField(
      controller: controller,
      enabled: enabled,
      keyboardType: const TextInputType.numberWithOptions(decimal: true),
      decoration: InputDecoration(labelText: label, isDense: true),
      onSubmitted: (text) {
        final value = double.tryParse(text.trim());
        if (value != null) onSubmit(value);
      },
    );
  }
}
