import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_database/firebase_database.dart';
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

  // ── RTDB Streams (real-time) ───────────────────────────────────

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

  // ── Commands (app → device) ───────────────────────────────────

  Future<void> sendManualFeed() =>
      _rtdb.ref('$_devicePath/commands/manualFeed').set(true);

  Future<void> sendManualWater() =>
      _rtdb.ref('$_devicePath/commands/manualWater').set(true);

  Future<void> sendEmergencyStop() =>
      _rtdb.ref('$_devicePath/commands/emergencyStop').set(true);

  Future<void> sendTare() =>
      _rtdb.ref('$_devicePath/commands/tare').set(true);

  Future<void> pushSettingsToDevice(Map<String, dynamic> settings) =>
      _rtdb.ref('$_devicePath/settings_cache').update(settings);

  // ── Feeding Schedules (Firestore) ─────────────────────────────

  Stream<List<FeedingSchedule>> schedulesStream() {
    return _firestore
        .collection('schedules')
        .where('deviceId', isEqualTo: AppConfig.deviceId)
        .orderBy('hour')
        .snapshots()
        .map((snap) =>
            snap.docs.map(FeedingSchedule.fromFirestore).toList());
  }

  Future<void> addSchedule(FeedingSchedule schedule) =>
      _firestore.collection('schedules').add(schedule.toMap());

  Future<void> updateSchedule(FeedingSchedule schedule) =>
      _firestore
          .collection('schedules')
          .doc(schedule.id)
          .update(schedule.toMap());

  Future<void> deleteSchedule(String id) =>
      _firestore.collection('schedules').doc(id).delete();

  Future<void> toggleSchedule(String id, bool enabled) =>
      _firestore.collection('schedules').doc(id).update({
        'enabled': enabled,
        'updatedAt': FieldValue.serverTimestamp(),
      });

  // ── Feeding Logs (Firestore) ──────────────────────────────────

  Stream<List<FeedingLog>> feedingLogsStream({int limit = 20}) {
    return _firestore
        .collection('feedingLogs')
        .where('deviceId', isEqualTo: AppConfig.deviceId)
        .orderBy('timestamp', descending: true)
        .limit(limit)
        .snapshots()
        .map((snap) => snap.docs.map(FeedingLog.fromFirestore).toList());
  }

  Future<void> saveFeedingLog(Map<String, dynamic> log) =>
      _firestore.collection('feedingLogs').add({
        ...log,
        'deviceId': AppConfig.deviceId,
        'timestamp': FieldValue.serverTimestamp(),
      });

  // ── AI Insights (Firestore) ───────────────────────────────────

  Stream<List<AIInsight>> insightsStream() {
    return _firestore
        .collection('aiInsights')
        .where('deviceId', isEqualTo: AppConfig.deviceId)
        .orderBy('generatedAt', descending: true)
        .limit(10)
        .snapshots()
        .map((snap) => snap.docs.map(AIInsight.fromFirestore).toList());
  }

  Future<void> markInsightRead(String id) =>
      _firestore.collection('aiInsights').doc(id).update({'isRead': true});

  // ── Notifications (Firestore) ─────────────────────────────────

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

  // ── Settings ──────────────────────────────────────────────────

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

  // ── RTDB pending notification listener ────────────────────────
  // ESP32 writes here; app picks it up and saves to Firestore
  Stream<Map<dynamic, dynamic>?> pendingNotificationStream() {
    return _rtdb
        .ref('$_devicePath/pendingNotification')
        .onValue
        .map((event) => event.snapshot.value as Map<dynamic, dynamic>?);
  }

  Future<void> clearPendingNotification() =>
      _rtdb.ref('$_devicePath/pendingNotification').remove();

  Future<void> saveNotificationToFirestore(
      String userId, Map<dynamic, dynamic> notif) =>
      _firestore.collection('notifications').add({
        ...Map<String, dynamic>.from(notif),
        'userId': userId,
        'isRead': false,
        'timestamp': FieldValue.serverTimestamp(),
      });
}
