import 'dart:io';
import 'package:cloud_firestore/cloud_firestore.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:flutter/material.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';

// ─────────────────────────────────────────────────────────────────────────────
// Background message handler — MUST be a top-level function (not a class method)
// ─────────────────────────────────────────────────────────────────────────────
@pragma('vm:entry-point')
Future<void> _firebaseMessagingBackgroundHandler(RemoteMessage message) async {
  // Firebase is already initialized when this fires; just log or process quietly.
  debugPrint('[FCM Background] ${message.notification?.title}: ${message.notification?.body}');
}

class NotificationService {
  NotificationService._();
  static final NotificationService instance = NotificationService._();

  final FirebaseMessaging _fcm = FirebaseMessaging.instance;
  final FirebaseFirestore _firestore = FirebaseFirestore.instance;

  // Local notifications plugin (for foreground message display on Android)
  final FlutterLocalNotificationsPlugin _localNotifications =
      FlutterLocalNotificationsPlugin();

  // Android notification channel (must match the one in Cloud Function)
  static const AndroidNotificationChannel _channel = AndroidNotificationChannel(
    'pet_feeder_alerts',
    'Pet Feeder Alerts',
    description: 'Alerts from your smart pet feeder',
    importance: Importance.high,
  );

  // Optional: expose a stream so the app can react to foreground messages
  // (e.g., show a SnackBar or in-app banner).
  final ValueNotifier<RemoteMessage?> foregroundMessage = ValueNotifier(null);

  // ── Initialization ──────────────────────────────────────────────────────

  /// Call once from main.dart AFTER Firebase.initializeApp().
  Future<void> initialize({String? userId}) async {
    // 1. Register the background handler first.
    FirebaseMessaging.onBackgroundMessage(_firebaseMessagingBackgroundHandler);

    // 2. Request permission (iOS / macOS prompt; Android 13+ prompt).
    await _requestPermission();

    // 3. Set up local notifications (Android foreground display).
    await _setupLocalNotifications();

    // 4. Listen for messages while the app is in the foreground.
    FirebaseMessaging.onMessage.listen(_handleForegroundMessage);

    // 5. Handle notification tap when app was in background (not terminated).
    FirebaseMessaging.onMessageOpenedApp.listen(_handleNotificationTap);

    // 6. Check for the initial message if the app was launched from a notification.
    final initial = await _fcm.getInitialMessage();
    if (initial != null) _handleNotificationTap(initial);

    // 7. Get (or refresh) the FCM token and persist it.
    if (userId != null) {
      await registerToken(userId);
    }

    // 8. Listen for token refreshes (device re-registers with FCM).
    _fcm.onTokenRefresh.listen((newToken) async {
      if (userId != null) {
        await _saveToken(userId, newToken);
      }
    });
  }

  // ── Permission ──────────────────────────────────────────────────────────

  Future<void> _requestPermission() async {
    final settings = await _fcm.requestPermission(
      alert: true,
      badge: true,
      sound: true,
      provisional: false,
    );
    debugPrint('[FCM] Permission status: ${settings.authorizationStatus}');
  }

  // ── Token management ────────────────────────────────────────────────────

  /// Gets the current FCM token and saves it to Firestore under users/{userId}.
  /// Call this after the user signs in.
  Future<void> registerToken(String userId) async {
    try {
      final token = await _fcm.getToken();
      if (token != null) {
        await _saveToken(userId, token);
        debugPrint('[FCM] Token registered for user $userId');
      }
    } catch (e) {
      debugPrint('[FCM] registerToken error: $e');
    }
  }

  Future<void> _saveToken(String userId, String token) async {
    await _firestore.collection('users').doc(userId).set({
      'fcmToken': token,
      'fcmTokenUpdatedAt': FieldValue.serverTimestamp(),
      'platform': Platform.operatingSystem,
    }, SetOptions(merge: true));
  }

  /// Call this when the user signs out — removes stale token from Firestore
  /// so they stop receiving notifications after logout.
  Future<void> unregisterToken(String userId) async {
    try {
      await _fcm.deleteToken();
      await _firestore.collection('users').doc(userId).update({
        'fcmToken': FieldValue.delete(),
      });
      debugPrint('[FCM] Token unregistered for user $userId');
    } catch (e) {
      debugPrint('[FCM] unregisterToken error: $e');
    }
  }

  // ── Local notifications setup ───────────────────────────────────────────

  Future<void> _setupLocalNotifications() async {
    // Create the Android notification channel
    await _localNotifications
        .resolvePlatformSpecificImplementation<
            AndroidFlutterLocalNotificationsPlugin>()
        ?.createNotificationChannel(_channel);

    // Initialize the plugin
    const androidSettings =
        AndroidInitializationSettings('@mipmap/ic_launcher');
    const iosSettings = DarwinInitializationSettings(
      requestAlertPermission: false, // already requested via FCM
      requestBadgePermission: false,
      requestSoundPermission: false,
    );
    const initSettings = InitializationSettings(
      android: androidSettings,
      iOS: iosSettings,
    );

    await _localNotifications.initialize(
      initSettings,
      onDidReceiveNotificationResponse: (response) {
        debugPrint('[LocalNotif] Tapped: ${response.payload}');
      },
    );

    // On Android, tell FCM to show heads-up notifications via the local plugin
    // (FCM doesn't show notification UI on Android when app is in foreground).
    await _fcm.setForegroundNotificationPresentationOptions(
      alert: false, // handled manually below
      badge: true,
      sound: true,
    );
  }

  // ── Message handlers ────────────────────────────────────────────────────

  void _handleForegroundMessage(RemoteMessage message) {
    debugPrint('[FCM Foreground] ${message.notification?.title}');

    // Notify the app (e.g., show SnackBar via ValueNotifier)
    foregroundMessage.value = message;

    // Also show a local heads-up notification on Android
    if (Platform.isAndroid) {
      final notification = message.notification;
      final android = message.notification?.android;
      if (notification != null && android != null) {
        _localNotifications.show(
          notification.hashCode,
          notification.title,
          notification.body,
          NotificationDetails(
            android: AndroidNotificationDetails(
              _channel.id,
              _channel.name,
              channelDescription: _channel.description,
              importance: Importance.high,
              priority: Priority.high,
              icon: android.smallIcon ?? '@mipmap/ic_launcher',
            ),
          ),
        );
      }
    }
  }

  void _handleNotificationTap(RemoteMessage message) {
    debugPrint('[FCM Tap] ${message.data}');
    // You can use a global NavigatorKey here to push a route based on message.data
    // e.g.: navigatorKey.currentState?.pushNamed('/notifications');
  }
}
