import 'package:flutter/material.dart';

class PresetCard extends StatelessWidget {
  const PresetCard({
    super.key,
    required this.index,
    required this.active,
    required this.enabled,
    required this.onLoad,
    required this.onSave,
  });

  final int index;
  final bool active;
  final bool enabled;
  final VoidCallback onLoad;
  final Future<void> Function() onSave;

  @override
  Widget build(BuildContext context) {
    return Card(
      color: active ? Theme.of(context).colorScheme.secondaryContainer : null,
      child: Padding(
        padding: const EdgeInsets.all(8),
        child: Column(
          children: [
            Text('Perfil $index', style: Theme.of(context).textTheme.titleSmall),
            const SizedBox(height: 4),
            FilledButton(onPressed: enabled ? onLoad : null, child: const Text('Cargar')),
            TextButton(onPressed: enabled ? () => _confirmSave(context) : null, child: const Text('Guardar actual')),
          ],
        ),
      ),
    );
  }

  Future<void> _confirmSave(BuildContext context) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text('Sobrescribir perfil $index'),
        content: const Text(
          'Se van a guardar los valores actuales de KP/KD/Velocidad en este perfil, reemplazando lo que tenía.',
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancelar')),
          FilledButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Guardar')),
        ],
      ),
    );
    if (confirmed == true) await onSave();
  }
}
