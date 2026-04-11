import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import 'package:intl/intl.dart';

const String _deviceId = 'bitdoglab-01';
const int _minEpochMs = 946684800000; // 01/01/2000

class HistoryPage extends StatelessWidget {
  const HistoryPage({super.key});

  int? _parseInt(dynamic v) {
    if (v == null) return null;
    if (v is num) return v.toInt();
    return int.tryParse(v.toString());
  }

  int _extractBestTs(Map<String, dynamic> value) {
    final tsUnix = _parseInt(value['ts_unix_ms']);
    if (tsUnix != null && tsUnix >= _minEpochMs) return tsUnix;

    final ts = _parseInt(value['ts']);
    if (ts != null && ts >= _minEpochMs) return ts;

    return 0; // desconhecido / não-real
  }

  String formatTs(Map<String, dynamic> value) {
    final best = _extractBestTs(value);
    if (best <= 0) return 'Sem horário real';
    final dt = DateTime.fromMillisecondsSinceEpoch(best).toLocal();
    return DateFormat('dd/MM/yyyy HH:mm:ss').format(dt);
  }

  String eventPtBr(String state, String event) {
    final e = event.toUpperCase();
    final s = state.toUpperCase();

    if (e == 'SOUND') return 'ALARME - SOM';
    if (e == 'MOTION') return 'ALARME - MOVIMENTO';
    if (e == 'SYSTEM' && s.contains('DESARM')) return 'SISTEMA - DESARMADO';
    if (e == 'SYSTEM' && s.contains('ARM')) return 'SISTEMA - ARMADO';
    if (e == 'ALARM') return 'ALARME';
    return event; // fallback
  }

  @override
  Widget build(BuildContext context) {
    // prioriza timestamp real; fallback ainda funciona pela extração
    final query = FirebaseDatabase.instance
        .ref('devices/$_deviceId/alarm')
        .orderByChild('ts_unix_ms')
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
              final va = Map<String, dynamic>.from(a.value);
              final vb = Map<String, dynamic>.from(b.value);
              return _extractBestTs(vb).compareTo(_extractBestTs(va));
            });

          return ListView.separated(
            itemCount: entries.length,
            separatorBuilder: (context, index) => const Divider(height: 1),
            itemBuilder: (_, i) {
              final entry = entries[i];
              final value = Map<String, dynamic>.from(entry.value);

              final state = value['state']?.toString() ?? '-';
              final eventRaw = value['event']?.toString() ?? '-';
              final eventFriendly = eventPtBr(state, eventRaw);
              final dtText = formatTs(value);

              return ListTile(
                leading: const Icon(Icons.notification_important_outlined),
                title: Text(state),
                subtitle: Text('Evento = $eventFriendly • Data/Hora = $dtText'),
                trailing: Text('#${entry.key}'),
              );
            },
          );
        },
      ),
    );
  }
}
