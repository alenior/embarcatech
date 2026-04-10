import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import 'package:intl/intl.dart';

const String _deviceId = 'bitdoglab-01';

class HistoryPage extends StatelessWidget {
  const HistoryPage({super.key});

  String formatTs(dynamic ts) {
    if (ts == null) return '-';
    final n = (ts is num) ? ts.toInt() : int.tryParse(ts.toString());
    if (n == null) return '-';

    // espera epoch ms
    final dt = DateTime.fromMillisecondsSinceEpoch(n);
    return DateFormat('dd/MM/yyyy HH:mm:ss').format(dt);
  }

  @override
  Widget build(BuildContext context) {
    final query = FirebaseDatabase.instance
        .ref('devices/$_deviceId/alarm')
        .orderByChild('ts')
        .limitToLast(100);

    return Scaffold(
      appBar: AppBar(title: const Text('Histórico')),
      body: StreamBuilder<DatabaseEvent>(
        stream: query.onValue,
        builder: (context, snapshot) {
          if (snapshot.hasError) {
            return Center(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Text(
                  'Erro ao ler histórico:\n${snapshot.error}',
                  textAlign: TextAlign.center,
                ),
              ),
            );
          }

          if (!snapshot.hasData) {
            return const Center(child: CircularProgressIndicator());
          }

          final raw = snapshot.data!.snapshot.value;
          if (raw == null) {
            return const Center(child: Text('Sem eventos no histórico.'));
          }

          final map = Map<String, dynamic>.from(raw as Map);
          final entries = map.entries.toList()
            ..sort((a, b) {
              final ta = (a.value['ts'] ?? 0) as num;
              final tb = (b.value['ts'] ?? 0) as num;
              return tb.compareTo(ta);
            });

          return ListView.separated(
            itemCount: entries.length,
            separatorBuilder: (context, index) => const Divider(height: 1),
            itemBuilder: (_, i) {
              final entry = entries[i];
              final value = Map<String, dynamic>.from(entry.value);

              final state = value['state']?.toString() ?? '-';
              final event = value['event']?.toString() ?? '-';
              final ts = value['ts']?.toString() ?? '-';

              return ListTile(
                leading: const Icon(Icons.notification_important_outlined),
                title: Text(state),
                subtitle: Text('Evento = $event • Data/Hora = ${formatTs(ts)}'),
                trailing: Text('#${entry.key}'),
              );
            },
          );
        },
      ),
    );
  }
}
