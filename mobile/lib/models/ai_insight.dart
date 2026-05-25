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

    // Support both the legacy 'aiInsights' schema and the 'notifications' schema
    // that the ESP32 writes via pendingNotification → Firestore bridge.
    //   aiInsights:    message, generatedAt (Timestamp), severity, isActionable
    //   notifications: body,    timestamp   (Timestamp), — (defaults applied)
    final generatedAt = (data['generatedAt'] as Timestamp?)?.toDate() ??
        (data['timestamp'] as Timestamp?)?.toDate() ??
        DateTime.now();

    return AIInsight(
      id: doc.id,
      deviceId: data['deviceId'] as String? ?? '',
      type: data['type'] as String? ?? 'ai_insight',
      title: data['title'] as String? ?? '',
      message: data['message'] as String? ?? data['body'] as String? ?? '',
      severity: (data['severity'] as num?)?.toInt() ?? 2,
      isRead: data['isRead'] as bool? ?? false,
      isActionable: data['isActionable'] as bool? ?? true,
      actionLabel: data['actionLabel'] as String?,
      actionRoute: data['actionRoute'] as String?,
      generatedAt: generatedAt,
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
