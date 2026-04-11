import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import 'package:intl/intl.dart';

const _deviceId = 'bitdoglab-01';
const _minEpochMs = 946684800000; // 01/01/2000

class ControlPage extends StatefulWidget {
  const ControlPage({super.key});

  @override
  State<ControlPage> createState() => _ControlPageState();
}

class _ControlPageState extends State<ControlPage> {
  bool _sending = false;
  String _lastSent = '-';

  DatabaseReference get _controlRef =>
      FirebaseDatabase.instance.ref('devices/$_deviceId/control');

  DatabaseReference get _statusRef =>
      FirebaseDatabase.instance.ref('devices/$_deviceId/alarm_status/current');

  int? _parseInt(dynamic v) {
    if (v == null) return null;
    if (v is num) return v.toInt();
    return int.tryParse(v.toString());
  }

  String _formatTs(dynamic rawTsUnix, dynamic rawTsFallback) {
    final tsUnix = _parseInt(rawTsUnix);
    final ts = _parseInt(rawTsFallback);

    final chosen = (tsUnix != null && tsUnix >= _minEpochMs)
        ? tsUnix
        : (ts != null && ts >= _minEpochMs ? ts : null);

    if (chosen == null) return 'Sem horário real';
    final dt = DateTime.fromMillisecondsSinceEpoch(chosen).toLocal();
    return DateFormat('dd/MM/yyyy HH:mm:ss').format(dt);
  }

  Future<void> _send(String desired) async {
    setState(() => _sending = true);
    try {
      await _controlRef.set({
        'desired_state': desired, // ARMED / DISARMED
        'updated_at': DateTime.now().millisecondsSinceEpoch,
        'source': 'flutter_app',
      });

      setState(() => _lastSent = desired);

      if (!mounted) return;
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('Comando enviado: $desired')));
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('Falha ao enviar comando: $e')));
    } finally {
      if (mounted) setState(() => _sending = false);
    }
  }

  Widget _buildActionButtons(bool isWide) {
    final armBtn = FilledButton.icon(
      onPressed: _sending ? null : () => _send('ARMED'),
      icon: const Icon(Icons.lock),
      label: const Text('Rearmar'),
    );

    final disarmBtn = FilledButton.tonalIcon(
      onPressed: _sending ? null : () => _send('DISARMED'),
      icon: const Icon(Icons.lock_open),
      label: const Text('Desarmar'),
    );

    if (isWide) {
      return Row(
        children: [
          Expanded(child: armBtn),
          const SizedBox(width: 12),
          Expanded(child: disarmBtn),
        ],
      );
    }

    return Column(
      children: [
        SizedBox(width: double.infinity, child: armBtn),
        const SizedBox(height: 12),
        SizedBox(width: double.infinity, child: disarmBtn),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Controle')),
      body: LayoutBuilder(
        builder: (context, constraints) {
          final isWide = constraints.maxWidth >= 600;

          return StreamBuilder<DatabaseEvent>(
            stream: _statusRef.onValue,
            builder: (context, snapshot) {
              Map<String, dynamic>? statusData;
              if (snapshot.hasData && snapshot.data!.snapshot.value != null) {
                statusData = Map<String, dynamic>.from(
                  snapshot.data!.snapshot.value as Map,
                );
              }

              final status =
                  statusData?['status']?.toString() ?? 'DESCONHECIDO';
              final statusWhen = _formatTs(
                statusData?['ts_unix_ms'],
                statusData?['ts'],
              );

              return ListView(
                padding: const EdgeInsets.all(16),
                children: [
                  Card(
                    child: ListTile(
                      leading: const Icon(Icons.shield),
                      title: const Text('Status em tempo real'),
                      subtitle: Text('$status\nAtualizado: $statusWhen'),
                      isThreeLine: true,
                    ),
                  ),
                  const SizedBox(height: 12),
                  Card(
                    child: ListTile(
                      leading: const Icon(Icons.send),
                      title: const Text('Último comando enviado (app)'),
                      subtitle: Text(_lastSent),
                      trailing: _sending
                          ? const SizedBox(
                              width: 20,
                              height: 20,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            )
                          : null,
                    ),
                  ),
                  const SizedBox(height: 16),
                  _buildActionButtons(isWide),
                  const SizedBox(height: 12),
                  Text(
                    'Dica: com base no status atual, escolha o comando mais apropriado.',
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                ],
              );
            },
          );
        },
      ),
    );
  }
}
