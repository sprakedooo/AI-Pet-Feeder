class SensorData {
  final double bowlWeight;
  final int bowlWeightPercent;
  final int waterLevel;
  final String hopperStatus;
  final String reservoirStatus;
  final int reservoirLevel;
  final bool petDetected;
  final double petDistance;
  final DateTime lastUpdated;

  const SensorData({
    required this.bowlWeight,
    required this.bowlWeightPercent,
    required this.waterLevel,
    required this.hopperStatus,
    required this.reservoirStatus,
    required this.reservoirLevel,
    required this.petDetected,
    required this.petDistance,
    required this.lastUpdated,
  });

  factory SensorData.empty() => SensorData(
        bowlWeight: 0,
        bowlWeightPercent: 0,
        waterLevel: 0,
        hopperStatus: 'unknown',
        reservoirStatus: 'unknown',
        reservoirLevel: 0,
        petDetected: false,
        petDistance: 999,
        lastUpdated: DateTime.now(),
      );

  factory SensorData.fromMap(Map<dynamic, dynamic> map) => SensorData(
        bowlWeight: (map['bowlWeight'] as num?)?.toDouble() ?? 0,
        bowlWeightPercent: (map['bowlWeightPercent'] as num?)?.toInt() ?? 0,
        waterLevel: (map['waterLevel'] as num?)?.toInt() ?? 0,
        hopperStatus: map['hopperStatus'] as String? ?? 'unknown',
        reservoirStatus: map['reservoirStatus'] as String? ?? 'unknown',
        reservoirLevel: (map['reservoirLevel'] as num?)?.toInt() ?? 0,
        petDetected: map['petDetected'] as bool? ?? false,
        petDistance: (map['petDistance'] as num?)?.toDouble() ?? 999,
        lastUpdated: map['lastUpdated'] != null
            ? DateTime.fromMillisecondsSinceEpoch(
                (map['lastUpdated'] as num).toInt() * 1000)
            : DateTime.now(),
      );

  bool get isHopperOk => hopperStatus == 'ok';
  bool get isHopperLow => hopperStatus == 'low';
  bool get isHopperEmpty => hopperStatus == 'empty';
  bool get isReservoirOk => reservoirStatus == 'ok';
  bool get isReservoirEmpty => reservoirStatus == 'empty';
  bool get isWaterLow => waterLevel < 30;
}
