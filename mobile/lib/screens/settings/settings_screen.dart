import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../config/app_colors.dart';
import '../../config/app_config.dart';
import '../../providers/auth_provider.dart';
import '../../providers/device_provider.dart';
import '../../services/database_service.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key});

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  double _portionGrams = AppConfig.defaultPortionGrams;
  int _waterThreshold = 30;
  int _detectionDistance = 30;
  bool _autoMode = true;
  bool _notifications = true;
  bool _loading = true;

  final _db = DatabaseService();

  @override
  void initState() {
    super.initState();
    _loadSettings();
  }

  Future<void> _loadSettings() async {
    final data = await _db.getSettings();
    if (data != null && mounted) {
      setState(() {
        final feeding = data['feeding'] as Map<String, dynamic>? ?? {};
        final water = data['water'] as Map<String, dynamic>? ?? {};
        final system = data['system'] as Map<String, dynamic>? ?? {};
        _portionGrams = (feeding['targetPortionGrams'] as num?)?.toDouble() ??
            AppConfig.defaultPortionGrams;
        _waterThreshold = (water['waterLowThreshold'] as num?)?.toInt() ?? 30;
        _autoMode = system['autoMode'] as bool? ?? true;
        _notifications = system['notificationsEnabled'] as bool? ?? true;
        _loading = false;
      });
    } else {
      setState(() => _loading = false);
    }
  }

  Future<void> _saveSettings() async {
    final settings = {
      'feeding': {
        'targetPortionGrams': _portionGrams,
      },
      'water': {
        'waterLowThreshold': _waterThreshold,
      },
      'system': {
        'autoMode': _autoMode,
        'notificationsEnabled': _notifications,
      },
    };
    await _db.saveSettings(settings);
    await _db.pushSettingsToDevice({
      'targetPortionGrams': _portionGrams,
      'waterLowThreshold': _waterThreshold,
      'detectionDistance': _detectionDistance,
      'autoMode': _autoMode,
    });
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Settings saved!'),
          backgroundColor: AppColors.success,
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final auth = context.watch<AuthProvider>();
    final device = context.watch<DeviceProvider>();

    if (_loading) {
      return const Scaffold(
        body: Center(child: CircularProgressIndicator()),
      );
    }

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: const Text('Settings'),
        actions: [
          TextButton(
            onPressed: _saveSettings,
            child: const Text('Save', style: TextStyle(color: AppColors.primary)),
          ),
        ],
      ),
      body: ListView(
        children: [
          // Device info
          _SectionHeader('Device'),
          Card(
            margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
            child: Column(
              children: [
                _InfoTile(
                    label: 'Device ID',
                    value: AppConfig.deviceId,
                    icon: Icons.device_hub),
                const Divider(height: 1, indent: 56),
                _InfoTile(
                    label: 'Firmware',
                    value: device.deviceStatus.firmwareVersion,
                    icon: Icons.memory),
                const Divider(height: 1, indent: 56),
                _InfoTile(
                    label: 'IP Address',
                    value: device.deviceStatus.ipAddress.isEmpty
                        ? 'Offline'
                        : device.deviceStatus.ipAddress,
                    icon: Icons.wifi),
                const Divider(height: 1, indent: 56),
                _InfoTile(
                    label: 'Signal',
                    value: device.deviceStatus.signalStrength,
                    icon: Icons.signal_wifi_4_bar),
              ],
            ),
          ),

          // Feeding settings
          _SectionHeader('Feeding'),
          Card(
            margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Default Portion: ${_portionGrams.toStringAsFixed(0)}g',
                    style: const TextStyle(fontWeight: FontWeight.w500),
                  ),
                  Slider(
                    value: _portionGrams,
                    min: 20,
                    max: 300,
                    divisions: 28,
                    activeColor: AppColors.primary,
                    label: '${_portionGrams.toStringAsFixed(0)}g',
                    onChanged: (v) => setState(() => _portionGrams = v),
                  ),
                  ListTile(
                    contentPadding: EdgeInsets.zero,
                    title: const Text('Automatic Mode'),
                    subtitle: const Text(
                        'Auto-dispense food on schedule',
                        style: TextStyle(fontSize: 12)),
                    trailing: Switch.adaptive(
                      value: _autoMode,
                      activeColor: AppColors.primary,
                      onChanged: (v) => setState(() => _autoMode = v),
                    ),
                  ),
                ],
              ),
            ),
          ),

          // Water settings
          _SectionHeader('Water'),
          Card(
            margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'Low Water Threshold: $_waterThreshold%',
                    style: const TextStyle(fontWeight: FontWeight.w500),
                  ),
                  Slider(
                    value: _waterThreshold.toDouble(),
                    min: 10,
                    max: 60,
                    divisions: 10,
                    activeColor: AppColors.accent,
                    label: '$_waterThreshold%',
                    onChanged: (v) =>
                        setState(() => _waterThreshold = v.toInt()),
                  ),
                ],
              ),
            ),
          ),

          // Notifications
          _SectionHeader('Notifications'),
          Card(
            margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
            child: SwitchListTile.adaptive(
              value: _notifications,
              activeColor: AppColors.primary,
              onChanged: (v) => setState(() => _notifications = v),
              title: const Text('Push Notifications'),
              subtitle: const Text('Alerts for food, water, and AI insights',
                  style: TextStyle(fontSize: 12)),
            ),
          ),

          // Manual controls
          _SectionHeader('Maintenance'),
          Card(
            margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
            child: Column(
              children: [
                ListTile(
                  leading: const Icon(Icons.scale, color: AppColors.warning),
                  title: const Text('Tare Scale'),
                  subtitle: const Text('Re-zero the load cell',
                      style: TextStyle(fontSize: 12)),
                  trailing: const Icon(Icons.chevron_right),
                  onTap: () {
                    context.read<DeviceProvider>().sendTare();
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(content: Text('Tare command sent.')),
                    );
                  },
                ),
                const Divider(height: 1, indent: 56),
                ListTile(
                  leading: const Icon(Icons.stop_circle_outlined,
                      color: AppColors.danger),
                  title: const Text('Emergency Stop',
                      style: TextStyle(color: AppColors.danger)),
                  subtitle: const Text('Immediately stop all actuators',
                      style: TextStyle(fontSize: 12)),
                  trailing: const Icon(Icons.chevron_right),
                  onTap: () async {
                    final confirmed = await showDialog<bool>(
                      context: context,
                      builder: (_) => AlertDialog(
                        title: const Text('Emergency Stop'),
                        content: const Text(
                            'This will immediately stop all motors and pumps.'),
                        actions: [
                          TextButton(
                              onPressed: () =>
                                  Navigator.pop(context, false),
                              child: const Text('Cancel')),
                          ElevatedButton(
                            onPressed: () =>
                                Navigator.pop(context, true),
                            style: ElevatedButton.styleFrom(
                                backgroundColor: AppColors.danger),
                            child: const Text('STOP'),
                          ),
                        ],
                      ),
                    );
                    if (confirmed == true && context.mounted) {
                      context.read<DeviceProvider>().sendEmergencyStop();
                    }
                  },
                ),
              ],
            ),
          ),

          // Account
          _SectionHeader('Account'),
          Card(
            margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
            child: Column(
              children: [
                _InfoTile(
                    label: 'Email',
                    value: auth.user?.email ?? '',
                    icon: Icons.email_outlined),
                const Divider(height: 1, indent: 56),
                ListTile(
                  leading: const Icon(Icons.logout, color: AppColors.danger),
                  title: const Text('Sign Out',
                      style: TextStyle(color: AppColors.danger)),
                  onTap: () => auth.signOut(),
                ),
              ],
            ),
          ),
          const SizedBox(height: 40),
        ],
      ),
    );
  }
}

class _SectionHeader extends StatelessWidget {
  final String title;
  const _SectionHeader(this.title);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding:
          const EdgeInsets.only(left: 16, top: 20, bottom: 6, right: 16),
      child: Text(
        title.toUpperCase(),
        style: const TextStyle(
          fontSize: 11,
          fontWeight: FontWeight.w700,
          color: AppColors.textSecondary,
          letterSpacing: 1,
        ),
      ),
    );
  }
}

class _InfoTile extends StatelessWidget {
  final String label;
  final String value;
  final IconData icon;

  const _InfoTile(
      {required this.label, required this.value, required this.icon});

  @override
  Widget build(BuildContext context) {
    return ListTile(
      leading: Icon(icon, color: AppColors.textSecondary, size: 20),
      title: Text(label,
          style: const TextStyle(fontSize: 13, color: AppColors.textSecondary)),
      trailing: Text(value,
          style: const TextStyle(
              fontSize: 13,
              fontWeight: FontWeight.w500,
              color: AppColors.textPrimary)),
    );
  }
}
