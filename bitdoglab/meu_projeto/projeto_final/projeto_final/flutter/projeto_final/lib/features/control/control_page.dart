import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';

const _deviceId = 'bitdoglab-01';

class ControlPage extends StatefulWidget {
  const ControlPage({super.key});

  @override
  State<ControlPage> createState() => _ControlPageState();
}

class _ControlPageState extends State<ControlPage> {
  bool _sending = false;
  String _last = '-';

  Future<void> _send(String desired) async {
    setState(() => _sending = true);
    try {
      await FirebaseDatabase.instance.ref('devices/$_deviceId/control').set({
        'desired_state': desired,
        'updated_at': DateTime.now().millisecondsSinceEpoch,
        'source': 'flutter_app',
      });
      setState(() => _last = desired);
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('Comando enviado: $desired')));
      }
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('Falha ao enviar comando: $e')));
    } finally {
      if (mounted) setState(() => _sending = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Controle')),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            Card(
              child: ListTile(
                title: const Text('Último comando enviado'),
                subtitle: Text(_last),
              ),
            ),
            const SizedBox(height: 16),
            FilledButton.icon(
              onPressed: _sending ? null : () => _send('ARMED'),
              icon: const Icon(Icons.lock),
              label: const Text('Rearmar'),
            ),
            const SizedBox(height: 12),
            FilledButton.tonalIcon(
              onPressed: _sending ? null : () => _send('DISARMED'),
              icon: const Icon(Icons.lock_open),
              label: const Text('Desarmar'),
            ),
          ],
        ),
      ),
    );
  }
}
