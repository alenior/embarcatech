import 'package:firebase_database/firebase_database.dart';
import '../../core/constants.dart';

class HistoryRepository {
  final _db = FirebaseDatabase.instance.ref('$basePath/alarm');
  Query latest() => _db.orderByChild('ts_unix_ms').limitToLast(100);
}
