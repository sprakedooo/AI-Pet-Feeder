import 'dart:async';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/foundation.dart';
import '../config/app_config.dart';
import '../models/sensor_data.dart';
import '../models/device_status.dart';
import '../models/feeding_schedule.dart';
import '../models/feeding_log.dart';
import '../models/ai_insight.dart';

class DatabaseService {
  final FirebaseFirestore _firestore = FirebaseFirestore.instance;
  final FirebaseDatabase _rtdb = FirebaseDatabase.instance;

  String get _devicePath => 'devices/${AppConfig.deviceId}';

  // Broadcast stream — UI subscribes to show in-app alerts when the ESP32
  // posts a new pendingNotification (before the bridge moves it to Firestore).
  final _alertController =
      StreamController<Map<String, dynamic>>.broadcast();
  Stream<Map<String, dynamic>> get alertStream => _alertController.stream;

  // ── Bridge listener state ─────────────────────────────────────────
  StreamSubscription<DatabaseEvent>? _feedingLogBridgeSub;
  StreamSubscription<DatabaseEvent>? _notifBridgeSub;
  int _lastBridgedFeedingTimestamp = 0; // epoch seconds — dedup guard

  // ── RTDB Streams (real-time) ───────────────────────────────────────

  Stream<SensorData> sensorStream() {
    return _rtdb
        .ref('$_devicePath/sensors')
        .onValue
        .map((event) {
      final data = event.snapshot.value as Map<dynamic, dynamic>?;
      if (data == null) return SensorData.empty();
      return SensorData.fromMap(data);
    });
  }

  Stream<DeviceStatus> deviceStatusStream() {
    return _rtdb
        .ref(_devicePath)
        .onValue
        .map((event) {
      final data = event.snapshot.value as Map<dynamic, dynamic>?;
      if (data == null) return DeviceStatus.offline();
      final status = (data['status'] as Map<dynamic, dynamic>?) ?? {};
      final state = (data['state'] as Map<dynamic, dynamic>?) ?? {};
      return DeviceStatus.fromMap(status, state);
    });
  }

  // ── Commands (app → device) ────────────────────────────────────────

  Future<void> sendManualFeed() =>
      _rtdb.ref('$_devicePath/commands/manualFeed').set(true);

  Future<void> sendManualWater() =>
      _rtdb.ref('$_devicePath/commands/manualWater').set(true);

  Future<void> sendEmergencyStop() =>
      _rtdb.ref('$_devicePath/commands/emergencyStop').set(true);

  Future<void> sendTare() =>
      _rtdb.ref('$_devicePath/commands/tare').set(true);

  Future<void> sendResetWifi() =>
      _rtdb.ref('$_devicePath/commands/resetWifi').set(true);

  Future<void> pushSettingsToDevice(Map<String, dynamic> settings) =>
      _rtdb.ref('$_devicePath/settings_cache').update(settings);

  /// Write calibration values to settings_cache and trigger immediate reload.
  Future<void> pushCalibrationToDevice(Map<String, dynamic> calibration) async {
    await _rtdb.ref('$_devicePath/settings_cache/calibration').update(calibration);
    await _rtdb.ref('$_devicePath/commands/reloadSettings').set(true);
  }

  // ── Feeding Schedules (Firestore + RTDB sync) ──────────────────────

  Stream<List<FeedingSchedule>> schedulesStream() {
    // Note: no orderBy here — combining where('deviceId') + orderBy('hour')
    // would require a Firestore composite index. Sort in Dart instead.
    return _firestore
        .collection('schedules')
        .where('deviceId', isEqualTo: AppConfig.deviceId)
        .snapshots()
        .map((snap) {
          final list = snap.docs.map(FeedingSchedule.fromFirestore).toList();
          list.sort((a, b) {
            final hourCmp = a.hour.compareTo(b.hour);
            return hourCmp != 0 ? hourCmp : a.minute.compareTo(b.minute);
          });
          return list;
        });
  }

  Future<void> addSchedule(FeedingSchedule schedule) async {
    await _firestore.collection('schedules').add(schedule.toMap());
    await _syncSchedulesToRTDB();
  }

  Future<void> updateSchedule(FeedingSchedule schedule) async {
    await _firestore
        .collection('schedules')
        .doc(schedule.id)
        .update(schedule.toMap());
    await _syncSchedulesToRTDB();
  }

  Future<void> deleteSchedule(String id) async {
    await _firestore.collection('schedules').doc(id).delete();
    await _syncSchedulesToRTDB();
  }

  Future<void> toggleSchedule(String id, bool enabled) async {
    await _firestore.collection('schedules').doc(id).update({
      'enabled': enabled,
      'updatedAt': FieldValue.serverTimestamp(),
    });
    await _syncSchedulesToRTDB();
  }

  // Fetches all schedules for this device from Firestore and writes
  // them to RTDB so the ESP32 picks them up on the next settings poll.
  Future<void> _syncSchedulesToRTDB() async {
    try {
      final snap = await _firestore
          .collection('schedules')
          .where('deviceId', isEqualTo: AppConfig.deviceId)
          .get();

      // Convert to a compact list the ESP32 can parse.
      // Days are stored as a bitmask integer (bit0=Sun, bit1=Mon, ..., bit6=Sat)
      // to avoid Firebase RTDB's array-vs-object ambiguity on the ESP32 side.
      final schedules = snap.docs.map((doc) {
        final s = FeedingSchedule.fromFirestore(doc);
        return {
          'id':           s.id,
          'hour':         s.hour,
          'minute':       s.minute,
          'portionGrams': s.portionGrams,
          'enabled':      s.enabled,
          'label':        s.label,
          'daysMask':     _daysToBitmask(s.days),
        };
      }).toList();

      await _rtdb
          .ref('devices/${AppConfig.deviceId}/settings_cache/schedules')
          .set(schedules);

      // Stamp update time so ESP32 can detect change cheaply
      await _rtdb
          .ref('devices/${AppConfig.deviceId}/settings_cache/schedulesUpdatedAt')
          .set(DateTime.now().millisecondsSinceEpoch ~/ 1000);

      // Tell the ESP32 to reload schedules immediately instead of waiting
      // for the 5-minute periodic poll.
      await _rtdb
          .ref('devices/${AppConfig.deviceId}/commands/reloadSchedules')
          .set(true);
    } catch (e) {
      // Non-fatal — Firestore is the source of truth
      debugPrint('[DB] _syncSchedulesToRTDB error: $e');
    }
  }

  // Converts a list of day strings to a bitmask integer.
  // bit0=Sun, bit1=Mon, bit2=Tue, bit3=Wed, bit4=Thu, bit5=Fri, bit6=Sat
  int _daysToBitmask(List<String> days) {
    const map = {
      'sun': 0, 'mon': 1, 'tue': 2,
      'wed': 3, 'thu': 4, 'fri': 5, 'sat': 6,
    };
    int mask = 0;
    for (final d in days) {
      final bit = map[d.toLowerCase()];
      if (bit != null) mask |= (1 << bit);
    }
    return mask == 0 ? 0x7F : mask; // default to every day if nothing set
  }

  // ── Feeding Logs (Firestore) ───────────────────────────────────────

  Stream<List<FeedingLog>> feedingLogsStream({int limit = 20}) {
    // No orderBy — where('deviceId') + orderBy('timestamp') needs a composite
    // Firestore index. Sort descending in Dart and take the first [limit] items.
    return _firestore
        .collection('feedingLogs')
        .where('deviceId', isEqualTo: AppConfig.deviceId)
        .snapshots()
        .map((snap) {
          final list = snap.docs.map(FeedingLog.fromFirestore).toList();
          list.sort((a, b) => b.timestamp.compareTo(a.timestamp));
          return list.take(limit).toList();
        });
  }

  Future<void> saveFeedingLog(Map<String, dynamic> log) =>
      _firestore.collection('feedingLogs').add({
        ...log,
        'deviceId': AppConfig.deviceId,
        'timestamp': FieldValue.serverTimestamp(),
      });

  // ── AI Insights ────────────────────────────────────────────────────
  //
  // The ESP32 writes AI insights to RTDB as pendingNotification with
  // type == 'ai_insight'.  The bridge listener (startBridgeListeners)
  // picks these up and saves them to Firestore 'notifications'.
  // This stream queries that same 'notifications' collection filtered
  // by type so the UI receives them in real time without a separate
  // 'aiInsights' collection.

  Stream<List<AIInsight>> insightsStream() {
    // Query only by deviceId to avoid requiring a composite Firestore index
    // (where + orderBy on different fields needs an explicit index).
    // Filtering by type and sorting are done in Dart instead.
    return _firestore
        .collection('notifications')
        .where('deviceId', isEqualTo: AppConfig.deviceId)
        .snapshots()
        .map((snap) {
          final insights = snap.docs
              .map(AIInsight.fromFirestore)
              .where((i) => i.type == 'ai_insight')
              .toList()
            ..sort((a, b) => b.generatedAt.compareTo(a.generatedAt));
          return insights.take(10).toList();
        });
  }

  Future<void> markInsightRead(String id) =>
      _firestore.collection('notifications').doc(id).update({'isRead': true});

  // ── Notifications (Firestore) ──────────────────────────────────────

  Stream<QuerySnapshot> notificationsStream(String userId) {
    return _firestore
        .collection('notifications')
        .where('userId', isEqualTo: userId)
        .orderBy('timestamp', descending: true)
        .limit(30)
        .snapshots();
  }

  Future<void> markNotificationRead(String id) =>
      _firestore.collection('notifications').doc(id).update({'isRead': true});

  Future<void> markAllNotificationsRead(String userId) async {
    final batch = _firestore.batch();
    final unread = await _firestore
        .collection('notifications')
        .where('userId', isEqualTo: userId)
        .where('isRead', isEqualTo: false)
        .get();
    for (final doc in unread.docs) {
      batch.update(doc.reference, {'isRead': true});
    }
    await batch.commit();
  }

  // ── Settings ───────────────────────────────────────────────────────

  Future<Map<String, dynamic>?> getSettings() async {
    final doc = await _firestore
        .collection('settings')
        .doc(AppConfig.deviceId)
        .get();
    return doc.data();
  }

  Future<void> saveSettings(Map<String, dynamic> settings) =>
      _firestore.collection('settings').doc(AppConfig.deviceId).set({
        ...settings,
        'deviceId': AppConfig.deviceId,
        'updatedAt': FieldValue.serverTimestamp(),
      }, SetOptions(merge: true));

  // ── RTDB → Firestore Bridge Listeners ─────────────────────────────
  //
  // The ESP32 writes feeding logs to RTDB 'lastFeedingLog' and AI
  // notifications to RTDB 'pendingNotification'.  These listeners pick
  // up new data and persist it to Firestore so the app UI stays current
  // even when the ESP32 can't reach Firestore directly.
  //
  // Call startBridgeListeners(userId) once the user is authenticated.
  // Call stopBridgeListeners() on sign-out.

  void startBridgeListeners(String userId) {
    stopBridgeListeners(); // cancel any previous subs first

    // ── Bridge 1: lastFeedingLog RTDB → feedingLogs Firestore ─────────
    _feedingLogBridgeSub = _rtdb
        .ref('$_devicePath/lastFeedingLog')
        .onValue
        .listen((event) async {
      try {
        final raw = event.snapshot.value;
        if (raw == null) return;
        final data = Map<String, dynamic>.from(raw as Map);

        // Use the epoch 'timestamp' from ESP32 as a dedup key.
        final ts = (data['timestamp'] as num?)?.toInt() ?? 0;
        if (ts == 0 || ts == _lastBridgedFeedingTimestamp) return;
        _lastBridgedFeedingTimestamp = ts;

        // Use a deterministic doc ID so that if the app restarts and
        // re-processes the same RTDB node, the set() call is idempotent
        // (merge:true keeps the serverTimestamp from the first write).
        final docId = '${AppConfig.deviceId}_$ts';
        await _firestore.collection('feedingLogs').doc(docId).set({
          ...data,
          'deviceId':    AppConfig.deviceId,
          'triggerType': data['triggerType'] as String? ?? 'device',
          'timestamp':   FieldValue.serverTimestamp(),
        }, SetOptions(merge: true));
        debugPrint('[DB] Bridged feeding log ts=$ts → doc $docId to Firestore.');
      } catch (e) {
        debugPrint('[DB] feedingLog bridge error: $e');
      }
    });

    // ── Bridge 2: pendingNotification RTDB → notifications Firestore ──
    _notifBridgeSub = _rtdb
        .ref('$_devicePath/pendingNotification')
        .onValue
        .listen((event) async {
      try {
        final raw = event.snapshot.value;
        if (raw == null) return;
        final data = Map<String, dynamic>.from(raw as Map);

        // Must have a 'type' field to be valid.
        if (data['type'] == null) return;

        // Emit to the in-app alert stream so the UI can show a SnackBar
        // immediately — before we move the notification to Firestore.
        _alertController.add(data);

        // Save to Firestore 'notifications'.
        await _firestore.collection('notifications').add({
          ...data,
          'userId':    userId,
          'deviceId':  AppConfig.deviceId,
          'isRead':    false,
          'timestamp': FieldValue.serverTimestamp(),
        });

        // Clear the RTDB node so we don't re-process it on reconnect.
        await _rtdb.ref('$_devicePath/pendingNotification').remove();

        debugPrint('[DB] Bridged notification type=${data['type']} to Firestore.');
      } catch (e) {
        debugPrint('[DB] notification bridge error: $e');
      }
    });

    debugPrint('[DB] Bridge listeners started for user $userId.');
  }

  void stopBridgeListeners() {
    _feedingLogBridgeSub?.cancel();
    _notifBridgeSub?.cancel();
    _feedingLogBridgeSub = null;
    _notifBridgeSub = null;
    debugPrint('[DB] Bridge listeners stopped.');
  }

  void dispose() {
    stopBridgeListeners();
    _alertController.close();
  }
}
