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
  double _portionGrams = AppConfig.defaultPortionGrams.clamp(20.0, 60.0);
  int _waterThreshold = 30;
  int _detectionDistance = 30;
  bool _autoMode = true;
  bool _notifications = true;
  bool _loading = true;

  // Calibration fields
  final _waterEmptyCtrl      = TextEditingController(text: '100');
  final _waterFullCtrl       = TextEditingController(text: '1800');
  final _reservoirEmptyCtrl  = TextEditingController(text: '100');
  final _reservoirFullCtrl   = TextEditingController(text: '1800');
  final _loadCellFactorCtrl  = TextEditingController(text: '420.0');
  bool _calibSaving = false;

  final _db = DatabaseService();

  @override
  void initState() {
    super.initState();
    _loadSettings();
  }

  @override
  void dispose() {
    _waterEmptyCtrl.dispose();
    _waterFullCtrl.dispose();
    _reservoirEmptyCtrl.dispose();
    _reservoirFullCtrl.dispose();
    _loadCellFactorCtrl.dispose();
    super.dispose();
  }

  Future<void> _loadSettings() async {
    try {
      final data = await _db.getSettings();
      if (!mounted) return;
      if (data != null) {
        setState(() {
          final feeding = data['feeding'] as Map<String, dynamic>? ?? {};
          final water = data['water'] as Map<String, dynamic>? ?? {};
          final system = data['system'] as Map<String, dynamic>? ?? {};
          final calib = data['calibration'] as Map<String, dynamic>? ?? {};
          _portionGrams = ((feeding['targetPortionGrams'] as num?)?.toDouble() ??
              AppConfig.defaultPortionGrams).clamp(20.0, 60.0);
          _waterThreshold = (water['waterLowThreshold'] as num?)?.toInt() ?? 30;
          _autoMode = system['autoMode'] as bool? ?? true;
          _notifications = system['notificationsEnabled'] as bool? ?? true;
          // Calibration
          if (calib['waterEmptyADC'] != null)
            _waterEmptyCtrl.text = calib['waterEmptyADC'].toString();
          if (calib['waterFullADC'] != null)
            _waterFullCtrl.text = calib['waterFullADC'].toString();
          if (calib['reservoirEmptyADC'] != null)
            _reservoirEmptyCtrl.text = calib['reservoirEmptyADC'].toString();
          if (calib['reservoirFullADC'] != null)
            _reservoirFullCtrl.text = calib['reservoirFullADC'].toString();
          if (calib['loadCellFactor'] != null)
            _loadCellFactorCtrl.text = calib['loadCellFactor'].toString();
          _loading = false;
        });
      } else {
        // No saved settings yet — show defaults
        setState(() => _loading = false);
      }
    } catch (e) {
      debugPrint('[Settings] _loadSettings error: $e');
      if (mounted) setState(() => _loading = false);
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

  void _showChangeWifiDialog(BuildContext context) {
    showDialog(
      context: context,
      builder: (_) => AlertDialog(
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
        title: const Row(
          children: [
            Icon(Icons.wifi, color: AppColors.primary),
            SizedBox(width: 8),
            Text('Change WiFi Network'),
          ],
        ),
        content: const Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              'This will restart the feeder in setup mode. Here\'s how it works:',
              style: TextStyle(fontWeight: FontWeight.w500),
            ),
            SizedBox(height: 12),
            _StepRow(number: '1', text: 'Tap Confirm — the feeder will restart.'),
            SizedBox(height: 6),
            _StepRow(number: '2', text: 'On your phone, connect to WiFi:\n"PetFeeder-Setup"'),
            SizedBox(height: 6),
            _StepRow(number: '3', text: 'Open your browser and go to:\n192.168.4.1'),
            SizedBox(height: 6),
            _StepRow(number: '4', text: 'Enter your new WiFi name and password, then save.'),
            SizedBox(height: 6),
            _StepRow(number: '5', text: 'The feeder restarts and connects to the new network.'),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          ElevatedButton.icon(
            icon: const Icon(Icons.wifi_find),
            label: const Text('Confirm'),
            style: ElevatedButton.styleFrom(
              backgroundColor: AppColors.primary,
              foregroundColor: Colors.white,
            ),
            onPressed: () {
              Navigator.pop(context);
              context.read<DeviceProvider>().sendResetWifi();
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text(
                    'Feeder is restarting… Connect to "PetFeeder-Setup" WiFi.',
                  ),
                  backgroundColor: AppColors.primary,
                  duration: Duration(seconds: 6),
                ),
              );
            },
          ),
        ],
      ),
    );
  }

  Future<void> _saveCalibration() async {
    final waterEmpty  = int.tryParse(_waterEmptyCtrl.text.trim());
    final waterFull   = int.tryParse(_waterFullCtrl.text.trim());
    final resEmpty    = int.tryParse(_reservoirEmptyCtrl.text.trim());
    final resFull     = int.tryParse(_reservoirFullCtrl.text.trim());
    final loadFactor  = double.tryParse(_loadCellFactorCtrl.text.trim());

    if (waterEmpty == null || waterFull == null || resEmpty == null ||
        resFull == null || loadFactor == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Invalid values — check all fields'),
            backgroundColor: AppColors.danger),
      );
      return;
    }
    if (waterFull <= waterEmpty || resFull <= resEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Full ADC must be greater than empty ADC'),
            backgroundColor: AppColors.danger),
      );
      return;
    }

    setState(() => _calibSaving = true);
    try {
      final calibMap = {
        'waterEmptyADC':    waterEmpty,
        'waterFullADC':     waterFull,
        'reservoirEmptyADC': resEmpty,
        'reservoirFullADC':  resFull,
        'loadCellFactor':   loadFactor,
      };
      // Persist to Firestore for UI reload
      await _db.saveSettings({'calibration': calibMap});
      // Push to RTDB so the ESP32 picks it up on its 5-min settings cycle
      await _db.pushCalibrationToDevice(calibMap);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Calibration saved — device will apply within seconds'),
              backgroundColor: AppColors.success),
        );
      }
    } finally {
      if (mounted) setState(() => _calibSaving = false);
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
                    max: 60,
                    divisions: 8,
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

          // Debug / Calibration
          _SectionHeader('Debug'),
          Card(
            margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
            child: ExpansionTile(
              leading: const Icon(Icons.tune, color: AppColors.warning),
              title: const Text('Sensor Calibration',
                  style: TextStyle(fontWeight: FontWeight.w600)),
              subtitle: const Text(
                  'ADC endpoints for 0–1.5 V sensors & load cell factor',
                  style: TextStyle(fontSize: 12, color: AppColors.textSecondary)),
              childrenPadding:
                  const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
              children: [
                // ── Water Bowl sensor ──
                const _CalibHeader('Water Bowl Sensor (GPIO 34)'),
                Row(children: [
                  Expanded(
                    child: _CalibField(
                      label: 'Empty ADC',
                      controller: _waterEmptyCtrl,
                      hint: '100',
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: _CalibField(
                      label: 'Full ADC',
                      controller: _waterFullCtrl,
                      hint: '1800',
                    ),
                  ),
                ]),
                const SizedBox(height: 12),
                // ── Reservoir sensor ──
                const _CalibHeader('Reservoir Sensor (GPIO 33)'),
                Row(children: [
                  Expanded(
                    child: _CalibField(
                      label: 'Empty ADC',
                      controller: _reservoirEmptyCtrl,
                      hint: '100',
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: _CalibField(
                      label: 'Full ADC',
                      controller: _reservoirFullCtrl,
                      hint: '1800',
                    ),
                  ),
                ]),
                const SizedBox(height: 12),
                // ── Load cell ──
                const _CalibHeader('HX711 Load Cell'),
                _CalibField(
                  label: 'Calibration Factor',
                  controller: _loadCellFactorCtrl,
                  hint: '420.0',
                  isDecimal: true,
                ),
                const SizedBox(height: 4),
                Padding(
                  padding: const EdgeInsets.only(bottom: 4),
                  child: Text(
                    'Tip: Open Serial Monitor to see live ADC values.\n'
                    'Read ADC when bowl is empty → set Empty ADC.\n'
                    'Fill to maximum → set Full ADC.\n'
                    'Device re-applies values within 5 minutes.',
                    style: const TextStyle(
                        fontSize: 11, color: AppColors.textSecondary),
                  ),
                ),
                SizedBox(
                  width: double.infinity,
                  child: ElevatedButton.icon(
                    onPressed: _calibSaving ? null : _saveCalibration,
                    icon: _calibSaving
                        ? const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(strokeWidth: 2))
                        : const Icon(Icons.save_outlined),
                    label: Text(_calibSaving ? 'Saving…' : 'Save Calibration'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: AppColors.warning,
                      foregroundColor: Colors.white,
                    ),
                  ),
                ),
                const SizedBox(height: 8),
              ],
            ),
          ),

          // Device
          _SectionHeader('Device'),
          Card(
            margin: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
            child: ListTile(
              leading: const Icon(Icons.wifi, color: AppColors.primary),
              title: const Text('Change WiFi Network'),
              subtitle: const Text('Switch the feeder to a different network'),
              trailing: const Icon(Icons.chevron_right),
              onTap: () => _showChangeWifiDialog(context),
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

/// Small bold sub-heading inside the calibration expansion panel.
class _CalibHeader extends StatelessWidget {
  final String text;
  const _CalibHeader(this.text);

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Text(
        text,
        style: const TextStyle(
          fontSize: 12,
          fontWeight: FontWeight.w700,
          color: AppColors.textSecondary,
          letterSpacing: 0.4,
        ),
      ),
    );
  }
}

/// Numbered step row used in the Change WiFi dialog.
class _StepRow extends StatelessWidget {
  final String number;
  final String text;
  const _StepRow({required this.number, required this.text});

  @override
  Widget build(BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          width: 22,
          height: 22,
          decoration: const BoxDecoration(
            color: AppColors.primary,
            shape: BoxShape.circle,
          ),
          alignment: Alignment.center,
          child: Text(number,
              style: const TextStyle(
                  color: Colors.white,
                  fontSize: 11,
                  fontWeight: FontWeight.bold)),
        ),
        const SizedBox(width: 10),
        Expanded(
          child: Text(text,
              style: const TextStyle(fontSize: 13, height: 1.4)),
        ),
      ],
    );
  }
}

/// Numeric text field used inside the calibration panel.
class _CalibField extends StatelessWidget {
  final String label;
  final TextEditingController controller;
  final String hint;
  final bool isDecimal;

  const _CalibField({
    required this.label,
    required this.controller,
    required this.hint,
    this.isDecimal = false,
  });

  @override
  Widget build(BuildContext context) {
    return TextField(
      controller: controller,
      decoration: InputDecoration(
        labelText: label,
        hintText: hint,
        isDense: true,
        border: const OutlineInputBorder(),
        contentPadding:
            const EdgeInsets.symmetric(horizontal: 10, vertical: 10),
      ),
      keyboardType: isDecimal
          ? const TextInputType.numberWithOptions(decimal: true)
          : TextInputType.number,
      style: const TextStyle(fontSize: 13),
    );
  }
}
