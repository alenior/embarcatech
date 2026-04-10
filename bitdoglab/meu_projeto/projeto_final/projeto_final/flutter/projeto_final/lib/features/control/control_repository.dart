import 'package:firebase_database/firebase_database.dart';
import '../../core/constants.dart';

class ControlRepository {
  final _db = FirebaseDatabase.instance.ref('$basePath/control');

  Future<void> setDesiredState(String state, String source) async {
    await _db.set({
      'desired_state': state, // ARMED / DISARMED
      'updated_at': DateTime.now().millisecondsSinceEpoch,
      'source': source,
    });
  }
}
