class DeviceStatus {
  final bool online;
  final DateTime lastSeen;
  final String ipAddress;
  final String firmwareVersion;
  final int wifiRSSI;
  final String currentState;
  final bool isDispensing;
  final bool isPumping;
  final String lastError;

  const DeviceStatus({
    required this.online,
    required this.lastSeen,
    required this.ipAddress,
    required this.firmwareVersion,
    required this.wifiRSSI,
    required this.currentState,
    required this.isDispensing,
    required this.isPumping,
    required this.lastError,
  });

  factory DeviceStatus.offline() => DeviceStatus(
        online: false,
        lastSeen: DateTime.now(),
        ipAddress: '',
        firmwareVersion: '',
        wifiRSSI: 0,
        currentState: 'OFFLINE',
        isDispensing: false,
        isPumping: false,
        lastError: '',
      );

  factory DeviceStatus.fromMap(
      Map<dynamic, dynamic> status, Map<dynamic, dynamic> state) =>
      DeviceStatus(
        online: status['online'] as bool? ?? false,
        lastSeen: status['lastSeen'] != null
            ? DateTime.fromMillisecondsSinceEpoch(
                (status['lastSeen'] as num).toInt() * 1000)
            : DateTime.now(),
        ipAddress: status['ipAddress'] as String? ?? '',
        firmwareVersion: status['firmwareVersion'] as String? ?? '',
        wifiRSSI: (status['wifiRSSI'] as num?)?.toInt() ?? 0,
        currentState: state['currentState'] as String? ?? 'UNKNOWN',
        isDispensing: state['isDispensing'] as bool? ?? false,
        isPumping: state['isPumping'] as bool? ?? false,
        lastError: state['lastError'] as String? ?? '',
      );

  String get signalStrength {
    if (wifiRSSI >= -50) return 'Excellent';
    if (wifiRSSI >= -60) return 'Good';
    if (wifiRSSI >= -70) return 'Fair';
    return 'Poor';
  }
}
