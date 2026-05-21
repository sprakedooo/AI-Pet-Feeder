import 'package:cloud_firestore/cloud_firestore.dart';

class FeedingLog {
  final String id;
  final String deviceId;
  final String status;
  final double targetGrams;
  final double dispensedGrams;
  final double bowlBefore;
  final double bowlAfter;
  final int retryCount;
  final bool jamDetected;
  final String triggerType;
  final DateTime timestamp;

  const FeedingLog({
    required this.id,
    required this.deviceId,
    required this.status,
    required this.targetGrams,
    required this.dispensedGrams,
    required this.bowlBefore,
    required this.bowlAfter,
    required this.retryCount,
    required this.jamDetected,
    required this.triggerType,
    required this.timestamp,
  });

  factory FeedingLog.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return FeedingLog(
      id: doc.id,
      deviceId: data['deviceId'] as String? ?? '',
      status: data['status'] as String? ?? 'unknown',
      targetGrams: (data['targetGrams'] as num?)?.toDouble() ?? 0,
      dispensedGrams: (data['dispensedGrams'] as num?)?.toDouble() ?? 0,
      bowlBefore: (data['bowlBefore'] as num?)?.toDouble() ?? 0,
      bowlAfter: (data['bowlAfter'] as num?)?.toDouble() ?? 0,
      retryCount: (data['retryCount'] as num?)?.toInt() ?? 0,
      jamDetected: data['jamDetected'] as bool? ?? false,
      triggerType: data['triggerType'] as String? ?? 'scheduled',
      timestamp: (data['timestamp'] as Timestamp?)?.toDate() ?? DateTime.now(),
    );
  }

  bool get isSuccess => status == 'success';
  bool get isSkipped => status.startsWith('skipped');
  bool get isFailed => status.startsWith('failed');

  String get statusLabel {
    switch (status) {
      case 'success':
        return 'Success';
      case 'skipped_bowl_full':
        return 'Skipped (Bowl Full)';
      case 'skipped_hopper_empty':
        return 'Skipped (Hopper Empty)';
      case 'failed_timeout':
        return 'Failed (Timeout)';
      case 'failed_jam':
        return 'Failed (Jam)';
      case 'failed_no_change':
        return 'Failed (No Change)';
      default:
        return status;
    }
  }
}
