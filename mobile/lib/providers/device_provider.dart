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

  DeviceProvider(this._db) {
    _init();
  }

  SensorData get sensorData => _sensorData;
  DeviceStatus get deviceStatus => _deviceStatus;
  bool get isLoading => _isLoading;
  bool get isOnline => _deviceStatus.online;

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
  }

  Future<void> sendManualFeed() => _db.sendManualFeed();
  Future<void> sendManualWater() => _db.sendManualWater();
  Future<void> sendEmergencyStop() => _db.sendEmergencyStop();
  Future<void> sendTare() => _db.sendTare();

  @override
  void dispose() {
    _sensorSub?.cancel();
    _statusSub?.cancel();
    super.dispose();
  }
}
