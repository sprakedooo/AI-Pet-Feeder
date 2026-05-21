import 'package:cloud_firestore/cloud_firestore.dart';

class AIInsight {
  final String id;
  final String deviceId;
  final String type;
  final String title;
  final String message;
  final int severity;
  final bool isRead;
  final bool isActionable;
  final String? actionLabel;
  final String? actionRoute;
  final DateTime generatedAt;

  const AIInsight({
    required this.id,
    required this.deviceId,
    required this.type,
    required this.title,
    required this.message,
    required this.severity,
    required this.isRead,
    required this.isActionable,
    this.actionLabel,
    this.actionRoute,
    required this.generatedAt,
  });

  factory AIInsight.fromFirestore(DocumentSnapshot doc) {
    final data = doc.data() as Map<String, dynamic>;
    return AIInsight(
      id: doc.id,
      deviceId: data['deviceId'] as String? ?? '',
      type: data['type'] as String? ?? 'info',
      title: data['title'] as String? ?? '',
      message: data['message'] as String? ?? '',
      severity: (data['severity'] as num?)?.toInt() ?? 1,
      isRead: data['isRead'] as bool? ?? false,
      isActionable: data['isActionable'] as bool? ?? false,
      actionLabel: data['actionLabel'] as String?,
      actionRoute: data['actionRoute'] as String?,
      generatedAt:
          (data['generatedAt'] as Timestamp?)?.toDate() ?? DateTime.now(),
    );
  }

  String get severityLabel {
    switch (severity) {
      case 5:
        return 'Critical';
      case 4:
        return 'High';
      case 3:
        return 'Medium';
      case 2:
        return 'Low';
      default:
        return 'Info';
    }
  }
}
