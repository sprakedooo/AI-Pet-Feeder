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

  final List<Widget> _screens = [
    const _HomeTab(),
    const ScheduleScreen(),
    const HistoryScreen(),
    const InsightsScreen(),
    const SettingsScreen(),
  ];

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
                  // Device state banner
                  if (device.deviceStatus.isDispensing ||
                      device.deviceStatus.isPumping)
                    _ActivityBanner(
                      dispensing: device.deviceStatus.isDispensing,
                      pumping: device.deviceStatus.isPumping,
                    ),

                  // Sensor cards grid
                  const Text('Sensor Status',
                      style: TextStyle(
                          fontSize: 16,
                          fontWeight: FontWeight.w600,
                          color: AppColors.textPrimary)),
                  const SizedBox(height: 12),
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
                      SensorCard(
                        title: 'Food Hopper',
                        value: device.sensorData.hopperStatus.toUpperCase(),
                        subtitle: device.sensorData.isHopperEmpty
                            ? 'Please refill!'
                            : 'Monitoring',
                        icon: Icons.inventory_2_outlined,
                        gradientColors: AppColors.hopperGradient,
                        showWarning: device.sensorData.isHopperEmpty ||
                            device.sensorData.isHopperLow,
                      ),
                      SensorCard(
                        title: 'Pet Presence',
                        value: device.sensorData.petDetected ? 'HERE' : 'AWAY',
                        subtitle: device.sensorData.petDetected
                            ? 'Pet at feeder'
                            : '${device.sensorData.petDistance.toStringAsFixed(0)}cm away',
                        icon: Icons.pets,
                        gradientColors: AppColors.petGradient,
                      ),
                    ],
                  ),

                  const SizedBox(height: 24),

                  // Today's summary
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

                  // Manual control
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
            child:
                CircularProgressIndicator(color: Colors.white, strokeWidth: 2),
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
