import 'dart:async';
import 'package:flutter/foundation.dart';
import '../models/ai_insight.dart';
import '../services/database_service.dart';

class AIProvider extends ChangeNotifier {
  final DatabaseService _db;

  List<AIInsight> _insights = [];
  bool _loading = true;

  StreamSubscription<List<AIInsight>>? _sub;

  AIProvider(this._db) {
    _sub = _db.insightsStream().listen((data) {
      _insights = data;
      _loading = false;
      notifyListeners();
    });
  }

  List<AIInsight> get insights => _insights;
  bool get loading => _loading;
  int get unreadCount => _insights.where((i) => !i.isRead).length;

  List<AIInsight> get criticalInsights =>
      _insights.where((i) => i.severity >= 4).toList();

  Future<void> markRead(String id) => _db.markInsightRead(id);

  @override
  void dispose() {
    _sub?.cancel();
    super.dispose();
  }
}
