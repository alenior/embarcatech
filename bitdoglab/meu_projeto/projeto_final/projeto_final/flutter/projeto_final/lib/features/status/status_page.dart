import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import 'package:intl/intl.dart';

const String _deviceId = 'bitdoglab-01';

class StatusPage extends StatelessWidget {
  const StatusPage({super.key});

  String formatTs(dynamic rawTsUnixMs, dynamic rawTsFallback) {
    int? parse(dynamic v) {
      if (v == null) return null;
      if (v is num) return v.toInt();
      return int.tryParse(v.toString());
    }

    // 1) prioridade para timestamp real (server/app)
    final tsUnix = parse(rawTsUnixMs);

    // 2) fallback para ts legado do firmware
    final tsAny = parse(rawTsFallback);

    // 01/01/2000 em ms -> limiar para considerar epoch real
    const minEpochMs = 946684800000;

    if (tsUnix != null && tsUnix >= minEpochMs) {
      final dt = DateTime.fromMillisecondsSinceEpoch(tsUnix).toLocal();
      return DateFormat('dd/MM/yyyy HH:mm:ss').format(dt);
    }

    if (tsAny != null && tsAny >= minEpochMs) {
      final dt = DateTime.fromMillisecondsSinceEpoch(tsAny).toLocal();
      return DateFormat('dd/MM/yyyy HH:mm:ss').format(dt);
    }

    // Não inventar data errada se vier uptime do firmware
    return 'Sem horário real (ts do dispositivo)';
  }

  @override
  Widget build(BuildContext context) {
    final ref = FirebaseDatabase.instance.ref(
      'devices/$_deviceId/alarm_status/current',
    );

    return Scaffold(
      appBar: AppBar(title: const Text('Status Atual')),
      body: StreamBuilder<DatabaseEvent>(
        stream: ref.onValue,
        builder: (context, snapshot) {
          if (snapshot.hasError) {
            return Center(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Text(
                  'Erro ao ler status:\n${snapshot.error}',
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
            return const Center(child: Text('Sem status disponível.'));
          }

          final data = Map<String, dynamic>.from(raw as Map);

          final status = data['status']?.toString() ?? '-';
          final fw = data['fw_version']?.toString() ?? '-';
          final wifiOk = data['wifi_ok']?.toString() ?? '-';
          final deviceId = data['device_id']?.toString() ?? _deviceId;

          return ListView(
            padding: const EdgeInsets.all(16),
            children: [
              Card(
                child: ListTile(
                  leading: const Icon(Icons.shield),
                  title: const Text('Status'),
                  subtitle: Text(status),
                ),
              ),
              Card(
                child: ListTile(
                  leading: const Icon(Icons.schedule),
                  title: const Text('Data/Hora'),
                  subtitle: Text(formatTs(data['ts_unix_ms'], data['ts'])),
                ),
              ),
              Card(
                child: ListTile(
                  leading: const Icon(Icons.memory),
                  title: const Text('Device ID'),
                  subtitle: Text(deviceId),
                ),
              ),
              Card(
                child: ListTile(
                  leading: const Icon(Icons.tag),
                  title: const Text('FW Version'),
                  subtitle: Text(fw),
                ),
              ),
              Card(
                child: ListTile(
                  leading: const Icon(Icons.wifi),
                  title: const Text('WiFi OK'),
                  subtitle: Text(wifiOk),
                ),
              ),
            ],
          );
        },
      ),
    );
  }
}
