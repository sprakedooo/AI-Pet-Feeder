import 'package:flutter/material.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:provider/provider.dart';
import 'app.dart';
import 'providers/auth_provider.dart';
import 'providers/device_provider.dart';
import 'providers/feeding_provider.dart';
import 'providers/ai_provider.dart';
import 'services/auth_service.dart';
import 'services/database_service.dart';
import 'services/notification_service.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp();

  // Initialize FCM (background handler registered here before any UI)
  await NotificationService.instance.initialize();

  final dbService = DatabaseService();
  final authService = AuthService();

  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => AuthProvider(authService)),
        ChangeNotifierProvider(create: (_) => DeviceProvider(dbService)),
        ChangeNotifierProvider(create: (_) => FeedingProvider(dbService)),
        ChangeNotifierProvider(create: (_) => AIProvider(dbService)),
      ],
      child: const PetFeederApp(),
    ),
  );
}
