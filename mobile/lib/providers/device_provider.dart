import 'dart:async';
import 'package:flutter/foundation.dart';
import '../models/sensor_data.dart';
import '../models/device_status.dart';
import '../services/database_service.dart';

class DeviceProvider extends ChangeNotifier {
  final DatabaseService _db;

  SensorData _sensorData = SensorData.empty();
  DeviceStatus _deviceStatus = DeviceStatus.offline();
  bool _isLoading = true;

  StreamSubscription<SensorData>? _sensorSub;
  StreamSubscription<DeviceStatus>? _statusSub;

  // RTDB only emits events when data *changes*. If the ESP32 goes silent,
  // no new events arrive and the UI stays frozen on the last known state.
  // This timer forces a rebuild every 10 s so both freshness checks
  // (heartbeat + sensor data) re-run and the "Offline" banner appears fast.
  Timer? _stalenessTimer;

  DeviceProvider(this._db) {
    _init();
  }

  SensorData get sensorData => _sensorData;
  DeviceStatus get deviceStatus => _deviceStatus;
  bool get isLoading => _isLoading;

  // Sensor data is pushed every 10 s — if it's older than 25 s the device
  // is silent regardless of what the heartbeat flag says.
  static const _sensorFreshnessThreshold = Duration(seconds: 25);

  /// Device is considered online when EITHER signal is fresh:
  ///  • heartbeat timestamp (pushed every 15 s, threshold 25 s), OR
  ///  • sensor data timestamp (pushed every 10 s, threshold 25 s).
  /// Both must go stale before we declare offline — this avoids false
  /// negatives from a single delayed push.
  bool get isOnline {
    if (_deviceStatus.isActuallyOnline) return true;
    final sensorAge = DateTime.now().difference(_sensorData.lastUpdated);
    return sensorAge <= _sensorFreshnessThreshold;
  }

  void _init() {
    _sensorSub = _db.sensorStream().listen((data) {
      _sensorData = data;
      _isLoading = false;
      notifyListeners();
    });

    _statusSub = _db.deviceStatusStream().listen((status) {
      _deviceStatus = status;
      notifyListeners();
    });

    // Staleness poll: re-evaluate both freshness checks even with no new RTDB events.
    _stalenessTimer = Timer.periodic(const Duration(seconds: 10), (_) {
      notifyListeners();
    });
  }

  /// Stream of real-time notifications from the ESP32.
  /// Subscribe once (in initState) to show in-app SnackBar alerts.
  Stream<Map<String, dynamic>> get alertStream => _db.alertStream;

  Future<void> sendManualFeed() => _db.sendManualFeed();
  Future<void> sendManualWater() => _db.sendManualWater();
  Future<void> sendEmergencyStop() => _db.sendEmergencyStop();
  Future<void> sendTare() => _db.sendTare();
  Future<void> sendResetWifi() => _db.sendResetWifi();

  @override
  void dispose() {
    _sensorSub?.cancel();
    _statusSub?.cancel();
    _stalenessTimer?.cancel();
    super.dispose();
  }
}
