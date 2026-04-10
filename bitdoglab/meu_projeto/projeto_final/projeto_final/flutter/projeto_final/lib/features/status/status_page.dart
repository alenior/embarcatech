import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';
import 'package:intl/intl.dart';

const String _deviceId = 'bitdoglab-01';

class StatusPage extends StatelessWidget {
  const StatusPage({super.key});

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
                  subtitle: Text(formatTs(data['ts_unix_ms'] ?? data['ts'])),
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
