import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../config/app_colors.dart';
import '../../providers/device_provider.dart';
import '../../providers/feeding_provider.dart';
import '../../providers/ai_provider.dart';
import '../../providers/auth_provider.dart';
import '../../widgets/sensor_card.dart';
import '../../widgets/status_indicator.dart';
import '../../widgets/action_button.dart';
import '../schedule/schedule_screen.dart';
import '../history/history_screen.dart';
import '../insights/insights_screen.dart';
import '../settings/settings_screen.dart';

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  int _currentIndex = 0;
  StreamSubscription<Map<String, dynamic>>? _alertSub;

  final List<Widget> _screens = [
    const _HomeTab(),
    const ScheduleScreen(),
    const HistoryScreen(),
    const InsightsScreen(),
    const SettingsScreen(),
  ];

  @override
  void initState() {
    super.initState();
    // Subscribe after the first frame so the Provider tree is ready.
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _alertSub = context
          .read<DeviceProvider>()
          .alertStream
          .listen(_onDeviceAlert);
    });
  }

  @override
  void dispose() {
    _alertSub?.cancel();
    super.dispose();
  }

  void _onDeviceAlert(Map<String, dynamic> notif) {
    final type  = notif['type']  as String? ?? '';
    final title = notif['title'] as String? ?? '';
    final body  = notif['body']  as String? ?? '';

    // Map notification type → icon + colour
    IconData icon;
    Color    color;

    switch (type) {
      case 'feeding_skipped':
        icon  = Icons.no_food_outlined;
        color = Colors.orange.shade700;
        break;
      case 'food_empty':
      case 'food_low':
        icon  = Icons.inventory_2_outlined;
        color = Colors.deepOrange.shade600;
        break;
      case 'reservoir_empty':
      case 'water_failed':
        icon  = Icons.water_drop_outlined;
        color = Colors.blue.shade700;
        break;
      case 'feeding_failed':
        icon  = Icons.error_outline;
        color = AppColors.danger;
        break;
      case 'emergency_stop':
        icon  = Icons.warning_amber_rounded;
        color = Colors.red.shade800;
        break;
      case 'feeding_success':
      case 'water_refilled':
        // Routine success events — skip the popup (don't spam the user)
        return;
      default:
        icon  = Icons.notifications_outlined;
        color = AppColors.primary;
    }

    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        duration: const Duration(seconds: 5),
        backgroundColor: color,
        behavior: SnackBarBehavior.floating,
        margin: const EdgeInsets.fromLTRB(16, 0, 16, 16),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        content: Row(
          children: [
            Icon(icon, color: Colors.white, size: 22),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                mainAxisSize: MainAxisSize.min,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(title,
                      style: const TextStyle(
                          color: Colors.white,
                          fontWeight: FontWeight.w700,
                          fontSize: 13)),
                  if (body.isNotEmpty)
                    Text(body,
                        style: const TextStyle(
                            color: Colors.white70, fontSize: 11)),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: IndexedStack(index: _currentIndex, children: _screens),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _currentIndex,
        onDestinationSelected: (i) => setState(() => _currentIndex = i),
        destinations: [
          const NavigationDestination(
              icon: Icon(Icons.home_outlined),
              selectedIcon: Icon(Icons.home),
              label: 'Home'),
          const NavigationDestination(
              icon: Icon(Icons.schedule_outlined),
              selectedIcon: Icon(Icons.schedule),
              label: 'Schedule'),
          const NavigationDestination(
              icon: Icon(Icons.history_outlined),
              selectedIcon: Icon(Icons.history),
              label: 'History'),
          NavigationDestination(
              icon: Consumer<AIProvider>(
                builder: (_, ai, __) => Badge(
                  isLabelVisible: ai.unreadCount > 0,
                  label: Text('${ai.unreadCount}'),
                  child: const Icon(Icons.lightbulb_outlined),
                ),
              ),
              selectedIcon: const Icon(Icons.lightbulb),
              label: 'Insights'),
          const NavigationDestination(
              icon: Icon(Icons.settings_outlined),
              selectedIcon: Icon(Icons.settings),
              label: 'Settings'),
        ],
      ),
    );
  }
}

// ── Home Tab ─────────────────────────────────────────────────────────────────

class _HomeTab extends StatelessWidget {
  const _HomeTab();

  @override
  Widget build(BuildContext context) {
    final device = context.watch<DeviceProvider>();
    final feeding = context.watch<FeedingProvider>();
    final auth = context.watch<AuthProvider>();

    return Scaffold(
      backgroundColor: AppColors.background,
      body: RefreshIndicator(
        onRefresh: () async {},
        child: CustomScrollView(
          slivers: [
            SliverAppBar(
              floating: true,
              backgroundColor: Colors.white,
              title: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Hello, ${auth.user?.displayName?.split(' ').first ?? 'there'}!',
                    style: const TextStyle(
                        fontSize: 20, fontWeight: FontWeight.bold),
                  ),
                  const Text('Smart Pet Feeder',
                      style: TextStyle(
                          fontSize: 12, color: AppColors.textSecondary)),
                ],
              ),
              actions: [
                Padding(
                  padding: const EdgeInsets.only(right: 16),
                  child: StatusIndicator(online: device.isOnline),
                ),
              ],
            ),
            SliverPadding(
              padding: const EdgeInsets.all(16),
              sliver: SliverList(
                delegate: SliverChildListDelegate([
                  // ── Pet presence pill ─────────────────────────────────────
                  _PetPresenceChip(
                    petDetected: device.sensorData.petDetected,
                    petDistance: device.sensorData.petDistance,
                  ),

                  const SizedBox(height: 16),

                  // ── Offline banner ────────────────────────────────────────
                  if (!device.isOnline)
                    _OfflineBanner(
                        lastSeen: device.deviceStatus.lastSeenLabel),

                  // ── Activity banner ───────────────────────────────────────
                  if (device.isOnline &&
                      (device.deviceStatus.isDispensing ||
                          device.deviceStatus.isPumping))
                    _ActivityBanner(
                      dispensing: device.deviceStatus.isDispensing,
                      pumping: device.deviceStatus.isPumping,
                    ),

                  // ── Sensor cards ──────────────────────────────────────────
                  const Text('Bowl Sensors',
                      style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.w600,
                          color: AppColors.textPrimary)),
                  const SizedBox(height: 12),
                  // Top row — live bowl readings (gradient cards)
                  GridView.count(
                    crossAxisCount: 2,
                    shrinkWrap: true,
                    physics: const NeverScrollableScrollPhysics(),
                    mainAxisSpacing: 12,
                    crossAxisSpacing: 12,
                    childAspectRatio: 1.2,
                    children: [
                      SensorCard(
                        title: 'Food Bowl',
                        value:
                            '${device.sensorData.bowlWeight.toStringAsFixed(1)}g',
                        subtitle:
                            '${device.sensorData.bowlWeightPercent}% full',
                        icon: Icons.food_bank_outlined,
                        gradientColors: AppColors.foodGradient,
                        progress: device.sensorData.bowlWeightPercent / 100,
                      ),
                      SensorCard(
                        title: 'Water Bowl',
                        value: '${device.sensorData.waterLevel}%',
                        subtitle: device.sensorData.isWaterLow
                            ? 'Low — refilling'
                            : 'Level OK',
                        icon: Icons.water_drop_outlined,
                        gradientColors: AppColors.waterGradient,
                        progress: device.sensorData.waterLevel / 100,
                      ),
                    ],
                  ),
                  const SizedBox(height: 20),
                  // Supply section header
                  Row(
                    children: const [
                      Text('Supply Levels',
                          style: TextStyle(
                              fontSize: 16,
                              fontWeight: FontWeight.w600,
                              color: AppColors.textPrimary)),
                      SizedBox(width: 8),
                      Text('— refill when low',
                          style: TextStyle(
                              fontSize: 12,
                              color: AppColors.textSecondary)),
                    ],
                  ),
                  const SizedBox(height: 12),
                  // Supply cards — full-width horizontal style
                  _SupplyStatusCard(
                    title: 'Food Hopper',
                    statusLabel: device.sensorData.hopperStatus.toUpperCase(),
                    subtitle: device.sensorData.isHopperEmpty
                        ? 'Hopper is empty — please refill now'
                        : device.sensorData.isHopperLow
                            ? 'Running low — refill soon'
                            : 'Supply level looks good',
                    icon: Icons.inventory_2_rounded,
                    accentColor: const Color(0xFFFF8F00), // deep amber
                    progress: device.sensorData.isHopperEmpty
                        ? 0.05
                        : device.sensorData.isHopperLow
                            ? 0.35
                            : 0.80,
                    isWarning: device.sensorData.isHopperEmpty ||
                        device.sensorData.isHopperLow,
                    isEmpty: device.sensorData.isHopperEmpty,
                  ),
                  const SizedBox(height: 10),
                  _SupplyStatusCard(
                    title: 'Water Tank',
                    statusLabel: '${device.sensorData.reservoirLevel}%',
                    subtitle: device.sensorData.isReservoirEmpty
                        ? 'Tank is empty — please refill now'
                        : device.sensorData.reservoirLevel < 30
                            ? 'Running low — refill soon'
                            : 'Water supply is sufficient',
                    icon: Icons.water_drop_rounded,
                    accentColor: const Color(0xFF0097A7), // deep teal
                    progress: device.sensorData.reservoirLevel / 100,
                    isWarning: device.sensorData.isReservoirEmpty ||
                        device.sensorData.reservoirLevel < 30,
                    isEmpty: device.sensorData.isReservoirEmpty,
                  ),

                  const SizedBox(height: 24),

                  // ── Today's summary ───────────────────────────────────────
                  const Text('Today\'s Summary',
                      style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.w600,
                          color: AppColors.textPrimary)),
                  const SizedBox(height: 12),
                  _SummaryCard(
                    feedingCount: feeding.todayFeedingCount,
                    totalGrams: feeding.todayTotalGrams,
                    lastFeeding: feeding.lastFeeding?.timestamp,
                  ),

                  const SizedBox(height: 24),

                  // ── Manual control ────────────────────────────────────────
                  const Text('Manual Control',
                      style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.w600,
                          color: AppColors.textPrimary)),
                  const SizedBox(height: 12),
                  Row(
                    children: [
                      Expanded(
                        child: ActionButton(
                          label: 'Feed Now',
                          icon: Icons.restaurant,
                          color: AppColors.primary,
                          onPressed: device.isOnline
                              ? () => _confirmAction(
                                    context,
                                    'Manual Feed',
                                    'Dispense food now?',
                                    () => device.sendManualFeed(),
                                  )
                              : null,
                        ),
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: ActionButton(
                          label: 'Water Now',
                          icon: Icons.water_drop,
                          color: AppColors.accent,
                          onPressed: device.isOnline
                              ? () => _confirmAction(
                                    context,
                                    'Manual Water',
                                    'Refill water bowl now?',
                                    () => device.sendManualWater(),
                                  )
                              : null,
                        ),
                      ),
                    ],
                  ),
                  const SizedBox(height: 100),
                ]),
              ),
            ),
          ],
        ),
      ),
    );
  }

  void _confirmAction(BuildContext context, String title, String message,
      VoidCallback onConfirm) {
    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        title: Text(title),
        content: Text(message),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
        actions: [
          TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('Cancel')),
          ElevatedButton(
            onPressed: () {
              Navigator.pop(context);
              onConfirm();
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(
                  content: Text('$title command sent!'),
                  backgroundColor: AppColors.success,
                ),
              );
            },
            child: const Text('Confirm'),
          ),
        ],
      ),
    );
  }
}

// ── Pet Presence Chip ─────────────────────────────────────────────────────────

class _PetPresenceChip extends StatelessWidget {
  final bool petDetected;
  final double petDistance;

  const _PetPresenceChip({
    required this.petDetected,
    required this.petDistance,
  });

  @override
  Widget build(BuildContext context) {
    final color = petDetected ? AppColors.primary : AppColors.textSecondary;

    return Center(
      child: Container(
        padding:
            const EdgeInsets.symmetric(horizontal: 14, vertical: 8),
        decoration: BoxDecoration(
          color: petDetected
              ? AppColors.primary.withOpacity(0.10)
              : Colors.grey.withOpacity(0.08),
          borderRadius: BorderRadius.circular(30),
          border: Border.all(
            color: petDetected
                ? AppColors.primary.withOpacity(0.4)
                : Colors.grey.shade300,
            width: 1.5,
          ),
        ),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            // Circle avatar with paw icon
            Container(
              width: 34,
              height: 34,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                gradient: LinearGradient(
                  colors: petDetected
                      ? [AppColors.primary, AppColors.primaryDark]
                      : [Colors.grey.shade400, Colors.grey.shade500],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
              ),
              child: const Icon(Icons.pets, color: Colors.white, size: 18),
            ),
            const SizedBox(width: 10),
            // Label
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisSize: MainAxisSize.min,
              children: [
                Text(
                  petDetected ? 'Pet is here!' : 'No pet nearby',
                  style: TextStyle(
                    fontWeight: FontWeight.w600,
                    fontSize: 13,
                    color: color,
                  ),
                ),
                Text(
                  petDetected
                      ? 'At the feeder'
                      : '${petDistance.toStringAsFixed(0)} cm away',
                  style: const TextStyle(
                      fontSize: 10, color: AppColors.textSecondary),
                ),
              ],
            ),
            const SizedBox(width: 10),
            // Live indicator dot
            AnimatedContainer(
              duration: const Duration(milliseconds: 400),
              width: 9,
              height: 9,
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: color,
                boxShadow: petDetected
                    ? [
                        BoxShadow(
                          color: AppColors.primary.withOpacity(0.5),
                          blurRadius: 6,
                          spreadRadius: 1,
                        )
                      ]
                    : null,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

// ── Offline Banner ────────────────────────────────────────────────────────────

class _OfflineBanner extends StatelessWidget {
  final String lastSeen;
  const _OfflineBanner({required this.lastSeen});

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.only(bottom: 16),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      decoration: BoxDecoration(
        color: Colors.grey.shade800,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        children: [
          const Icon(Icons.wifi_off, color: Colors.white70, size: 20),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text('Device Offline',
                    style: TextStyle(
                        color: Colors.white,
                        fontWeight: FontWeight.w600,
                        fontSize: 13)),
                Text(
                  'Last seen $lastSeen — showing cached data',
                  style:
                      const TextStyle(color: Colors.white60, fontSize: 11),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

// ── Activity Banner ───────────────────────────────────────────────────────────

class _ActivityBanner extends StatelessWidget {
  final bool dispensing;
  final bool pumping;

  const _ActivityBanner({required this.dispensing, required this.pumping});

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.only(bottom: 16),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
      decoration: BoxDecoration(
        color: dispensing ? AppColors.primary : AppColors.accent,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        children: [
          const SizedBox(
            width: 20,
            height: 20,
            child: CircularProgressIndicator(
                color: Colors.white, strokeWidth: 2),
          ),
          const SizedBox(width: 12),
          Text(
            dispensing ? 'Dispensing food...' : 'Refilling water...',
            style: const TextStyle(
                color: Colors.white, fontWeight: FontWeight.w500),
          ),
        ],
      ),
    );
  }
}

// ── Summary Card ──────────────────────────────────────────────────────────────

class _SummaryCard extends StatelessWidget {
  final int feedingCount;
  final double totalGrams;
  final DateTime? lastFeeding;

  const _SummaryCard({
    required this.feedingCount,
    required this.totalGrams,
    this.lastFeeding,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            _StatItem(
              label: 'Feedings',
              value: '$feedingCount',
              icon: Icons.restaurant,
              color: AppColors.primary,
            ),
            const VerticalDivider(width: 32),
            _StatItem(
              label: 'Total Food',
              value: '${totalGrams.toStringAsFixed(0)}g',
              icon: Icons.scale,
              color: AppColors.warning,
            ),
            const VerticalDivider(width: 32),
            _StatItem(
              label: 'Last Fed',
              value: lastFeeding != null
                  ? '${lastFeeding!.hour.toString().padLeft(2, '0')}:${lastFeeding!.minute.toString().padLeft(2, '0')}'
                  : '--:--',
              icon: Icons.access_time,
              color: AppColors.accent,
            ),
          ],
        ),
      ),
    );
  }
}

class _StatItem extends StatelessWidget {
  final String label;
  final String value;
  final IconData icon;
  final Color color;

  const _StatItem(
      {required this.label,
      required this.value,
      required this.icon,
      required this.color});

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Column(
        children: [
          Icon(icon, color: color, size: 22),
          const SizedBox(height: 6),
          Text(value,
              style: const TextStyle(
                  fontWeight: FontWeight.bold,
                  fontSize: 18,
                  color: AppColors.textPrimary)),
          Text(label,
              style: const TextStyle(
                  fontSize: 11, color: AppColors.textSecondary)),
        ],
      ),
    );
  }
}

// ── Supply Status Card ────────────────────────────────────────────────────────
// Full-width horizontal card for hopper / water tank — visually distinct
// from the compact gradient SensorCards used for live bowl readings.

class _SupplyStatusCard extends StatelessWidget {
  final String title;
  final String statusLabel; // e.g. "OK", "LOW", "75%"
  final String subtitle;
  final IconData icon;
  final Color accentColor;
  final double progress;    // 0.0 – 1.0
  final bool isWarning;
  final bool isEmpty;

  const _SupplyStatusCard({
    required this.title,
    required this.statusLabel,
    required this.subtitle,
    required this.icon,
    required this.accentColor,
    required this.progress,
    this.isWarning = false,
    this.isEmpty = false,
  });

  Color get _statusColor {
    if (isEmpty) return AppColors.danger;
    if (isWarning) return AppColors.warning;
    return accentColor;
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.circular(16),
        border: Border(
          left: BorderSide(color: _statusColor, width: 5),
        ),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.07),
            blurRadius: 10,
            offset: const Offset(0, 3),
          ),
        ],
      ),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
      child: Row(
        children: [
          // Icon in a soft circle
          Container(
            width: 50,
            height: 50,
            decoration: BoxDecoration(
              shape: BoxShape.circle,
              color: _statusColor.withValues(alpha: 0.12),
            ),
            child: Icon(icon, color: _statusColor, size: 26),
          ),
          const SizedBox(width: 14),
          // Text content + progress bar
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Text(
                      title,
                      style: const TextStyle(
                          fontSize: 13,
                          fontWeight: FontWeight.w600,
                          color: AppColors.textPrimary),
                    ),
                    const Spacer(),
                    // Status badge pill
                    Container(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 10, vertical: 3),
                      decoration: BoxDecoration(
                        color: _statusColor.withValues(alpha: 0.13),
                        borderRadius: BorderRadius.circular(20),
                      ),
                      child: Text(
                        statusLabel,
                        style: TextStyle(
                            fontSize: 11,
                            fontWeight: FontWeight.w800,
                            color: _statusColor,
                            letterSpacing: 0.5),
                      ),
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                ClipRRect(
                  borderRadius: BorderRadius.circular(6),
                  child: LinearProgressIndicator(
                    value: progress.clamp(0.0, 1.0),
                    backgroundColor: Colors.grey.shade200,
                    valueColor: AlwaysStoppedAnimation<Color>(_statusColor),
                    minHeight: 7,
                  ),
                ),
                const SizedBox(height: 6),
                Text(
                  subtitle,
                  style: TextStyle(
                      fontSize: 11,
                      color: isWarning
                          ? _statusColor
                          : AppColors.textSecondary,
                      fontWeight: isWarning
                          ? FontWeight.w600
                          : FontWeight.normal),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
