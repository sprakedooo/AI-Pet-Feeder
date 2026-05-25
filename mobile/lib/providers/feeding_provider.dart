import 'dart:async';
import 'package:flutter/foundation.dart';
import '../models/feeding_schedule.dart';
import '../models/feeding_log.dart';
import '../services/database_service.dart';

class FeedingProvider extends ChangeNotifier {
  final DatabaseService _db;

  List<FeedingSchedule> _schedules = [];
  List<FeedingLog> _logs = [];
  bool _schedulesLoading = true;
  bool _logsLoading = true;

  StreamSubscription<List<FeedingSchedule>>? _schedulesSub;
  StreamSubscription<List<FeedingLog>>? _logsSub;

  FeedingProvider(this._db) {
    _init();
  }

  List<FeedingSchedule> get schedules => _schedules;
  List<FeedingLog> get logs => _logs;
  bool get schedulesLoading => _schedulesLoading;
  bool get logsLoading => _logsLoading;

  // Analytics
  double get todayTotalGrams {
    final today = DateTime.now();
    return _logs
        .where((l) =>
            l.timestamp.day == today.day &&
            l.timestamp.month == today.month &&
            l.timestamp.year == today.year &&
            l.isSuccess)
        .fold(0.0, (sum, l) => sum + l.dispensedGrams);
  }

  int get todayFeedingCount {
    final today = DateTime.now();
    return _logs
        .where((l) =>
            l.timestamp.day == today.day &&
            l.timestamp.month == today.month &&
            l.timestamp.year == today.year &&
            l.isSuccess)
        .length;
  }

  FeedingLog? get lastFeeding =>
      _logs.isNotEmpty ? _logs.first : null;

  void _init() {
    // Safety-net: stop spinners after 6 s even if streams never emit
    // (e.g. Firestore permission error, missing composite index, no data yet).
    Future.delayed(const Duration(seconds: 6), () {
      bool changed = false;
      if (_schedulesLoading) { _schedulesLoading = false; changed = true; }
      if (_logsLoading)      { _logsLoading      = false; changed = true; }
      if (changed) notifyListeners();
    });

    _schedulesSub = _db.schedulesStream().listen(
      (data) {
        _schedules = data;
        _schedulesLoading = false;
        notifyListeners();
      },
      onError: (e) {
        debugPrint('[FeedingProvider] schedulesStream error: $e');
        _schedulesLoading = false;
        notifyListeners();
      },
    );

    _logsSub = _db.feedingLogsStream().listen(
      (data) {
        _logs = data;
        _logsLoading = false;
        notifyListeners();
      },
      onError: (e) {
        debugPrint('[FeedingProvider] feedingLogsStream error: $e');
        _logsLoading = false;
        notifyListeners();
      },
    );
  }

  Future<void> addSchedule(FeedingSchedule schedule) =>
      _db.addSchedule(schedule);

  Future<void> updateSchedule(FeedingSchedule schedule) =>
      _db.updateSchedule(schedule);

  Future<void> deleteSchedule(String id) => _db.deleteSchedule(id);

  Future<void> toggleSchedule(String id, bool enabled) =>
      _db.toggleSchedule(id, enabled);

  @override
  void dispose() {
    _schedulesSub?.cancel();
    _logsSub?.cancel();
    super.dispose();
  }
}
