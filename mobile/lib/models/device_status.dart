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

  // Heartbeat is now every 15 s — declare offline after 25 s (gives one
  // missed heartbeat of slack for network jitter, then fails fast).
  static const _offlineThreshold = Duration(seconds: 25);

  /// True only when the Firebase `online` flag is set AND the ESP32's last
  /// heartbeat arrived within [_offlineThreshold].
  /// The raw [online] flag is never cleared by the device (it can't — it's
  /// offline!), so we must use the timestamp to detect a stale connection.
  bool get isActuallyOnline =>
      online && DateTime.now().difference(lastSeen) <= _offlineThreshold;

  /// Human-readable time since last heartbeat, e.g. "2 min ago".
  String get lastSeenLabel {
    final diff = DateTime.now().difference(lastSeen);
    if (diff.inSeconds < 60) return '${diff.inSeconds}s ago';
    if (diff.inMinutes < 60) return '${diff.inMinutes}m ago';
    return '${diff.inHours}h ago';
  }

  factory DeviceStatus.offline() => DeviceStatus(
        online: false,
        lastSeen: DateTime.fromMillisecondsSinceEpoch(0), // epoch → always stale
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
            : DateTime.fromMillisecondsSinceEpoch(0),
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
