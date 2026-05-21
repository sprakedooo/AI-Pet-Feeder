import 'package:cloud_firestore/cloud_firestore.dart';

class FeedingSchedule {
  final String id;
  final String deviceId;
  final String label;
  final int hour;
  final int minute;
  final bool enabled;
  final double portionGrams;
  final List<String> days;
  final DateTime createdAt;

  const FeedingSchedule({
    required this.id,
    required this.deviceId,
    required this.label,
    required this.hour,
    required this.minute,
    required this.enabled,
    required this.portionGrams,
    required this.days,
    required this.createdAt,
  });

  factory FeedingSchedule.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return FeedingSchedule(
      id: doc.id,
      deviceId: data['deviceId'] as String? ?? '',
      label: data['label'] as String? ?? 'Feeding',
      hour: (data['hour'] as num?)?.toInt() ?? 8,
      minute: (data['minute'] as num?)?.toInt() ?? 0,
      enabled: data['enabled'] as bool? ?? true,
      portionGrams: (data['portionGrams'] as num?)?.toDouble() ?? 80.0,
      days: List<String>.from(data['days'] as List? ?? [
        'mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'sun'
      ]),
      createdAt: (data['createdAt'] as Timestamp?)?.toDate() ?? DateTime.now(),
    );
  }

  Map<String, dynamic> toMap() => {
        'deviceId': deviceId,
        'label': label,
        'hour': hour,
        'minute': minute,
        'enabled': enabled,
        'portionGrams': portionGrams,
        'days': days,
        'createdAt': Timestamp.fromDate(createdAt),
        'updatedAt': FieldValue.serverTimestamp(),
      };

  String get timeString {
    final h = hour.toString().padLeft(2, '0');
    final m = minute.toString().padLeft(2, '0');
    return '$h:$m';
  }

  String get timeLabel {
    final h = hour % 12 == 0 ? 12 : hour % 12;
    final m = minute.toString().padLeft(2, '0');
    final period = hour < 12 ? 'AM' : 'PM';
    return '$h:$m $period';
  }

  FeedingSchedule copyWith({
    String? id,
    String? label,
    int? hour,
    int? minute,
    bool? enabled,
    double? portionGrams,
    List<String>? days,
  }) =>
      FeedingSchedule(
        id: id ?? this.id,
        deviceId: deviceId,
        label: label ?? this.label,
        hour: hour ?? this.hour,
        minute: minute ?? this.minute,
        enabled: enabled ?? this.enabled,
        portionGrams: portionGrams ?? this.portionGrams,
        days: days ?? this.days,
        createdAt: createdAt,
      );
}
