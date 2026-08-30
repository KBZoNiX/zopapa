import 'package:flutter/material.dart';

Future<void> confirmAndRun(
  BuildContext context, {
  required String title,
  required String message,
  required Future<void> Function() action,
}) async {
  final confirmed = await showDialog<bool>(
    context: context,
    builder: (ctx) => AlertDialog(
      title: Text(title),
      content: Text(message),
      actions: [
        TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancelar')),
        FilledButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Continuar')),
      ],
    ),
  );
  if (confirmed == true) await action();
}
