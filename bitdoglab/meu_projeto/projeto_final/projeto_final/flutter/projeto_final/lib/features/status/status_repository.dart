import 'package:firebase_database/firebase_database.dart';
import '../../core/constants.dart';

class StatusRepository {
  final _db = FirebaseDatabase.instance.ref('$basePath/alarm_status/current');
  Stream<DatabaseEvent> watchCurrent() => _db.onValue;
}
