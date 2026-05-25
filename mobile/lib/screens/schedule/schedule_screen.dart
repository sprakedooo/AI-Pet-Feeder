import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:uuid/uuid.dart';
import '../../config/app_colors.dart';
import '../../models/feeding_schedule.dart';
import '../../providers/feeding_provider.dart';
import '../../config/app_config.dart';

class ScheduleScreen extends StatelessWidget {
  const ScheduleScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<FeedingProvider>();

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: const Text('Feeding Schedule'),
        actions: [
          IconButton(
            icon: const Icon(Icons.add),
            onPressed: () => _showScheduleDialog(context, null),
          ),
        ],
      ),
      body: provider.schedulesLoading
          ? const Center(child: CircularProgressIndicator())
          : provider.schedules.isEmpty
              ? _EmptySchedule(
                  onAdd: () => _showScheduleDialog(context, null))
              : ListView.separated(
                  padding: const EdgeInsets.all(16),
                  itemCount: provider.schedules.length,
                  separatorBuilder: (_, __) => const SizedBox(height: 8),
                  itemBuilder: (context, i) {
                    final schedule = provider.schedules[i];
                    return _ScheduleTile(
                      schedule: schedule,
                      onToggle: (enabled) => provider.toggleSchedule(
                          schedule.id, enabled),
                      onEdit: () =>
                          _showScheduleDialog(context, schedule),
                      onDelete: () =>
                          provider.deleteSchedule(schedule.id),
                    );
                  },
                ),
    );
  }

  void _showScheduleDialog(
      BuildContext context, FeedingSchedule? existing) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      backgroundColor: Colors.transparent,
      builder: (_) => _ScheduleEditSheet(existing: existing),
    );
  }
}

class _ScheduleTile extends StatelessWidget {
  final FeedingSchedule schedule;
  final ValueChanged<bool> onToggle;
  final VoidCallback onEdit;
  final VoidCallback onDelete;

  const _ScheduleTile({
    required this.schedule,
    required this.onToggle,
    required this.onEdit,
    required this.onDelete,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      child: ListTile(
        contentPadding:
            const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        leading: Container(
          padding: const EdgeInsets.all(10),
          decoration: BoxDecoration(
            color: schedule.enabled
                ? AppColors.primary.withValues(alpha: 0.1)
                : Colors.grey.withValues(alpha: 0.1),
            borderRadius: BorderRadius.circular(10),
          ),
          child: Icon(
            Icons.restaurant,
            color: schedule.enabled ? AppColors.primary : Colors.grey,
          ),
        ),
        title: Text(schedule.label,
            style: const TextStyle(fontWeight: FontWeight.w600)),
        subtitle: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(schedule.timeLabel,
                style: const TextStyle(
                    fontSize: 20,
                    fontWeight: FontWeight.bold,
                    color: AppColors.textPrimary)),
            Text('${schedule.portionGrams.toStringAsFixed(0)}g • ${_daysLabel(schedule.days)}',
                style: const TextStyle(
                    fontSize: 12, color: AppColors.textSecondary)),
          ],
        ),
        trailing: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Switch.adaptive(
              value: schedule.enabled,
              activeColor: AppColors.primary,
              onChanged: onToggle,
            ),
            PopupMenuButton(
              itemBuilder: (_) => [
                const PopupMenuItem(value: 'edit', child: Text('Edit')),
                const PopupMenuItem(
                    value: 'delete',
                    child: Text('Delete',
                        style: TextStyle(color: AppColors.danger))),
              ],
              onSelected: (v) {
                if (v == 'edit') onEdit();
                if (v == 'delete') onDelete();
              },
            ),
          ],
        ),
      ),
    );
  }

  String _daysLabel(List<String> days) {
    if (days.length == 7) return 'Every day';
    if (days.length == 5 &&
        !days.contains('sat') &&
        !days.contains('sun')) return 'Weekdays';
    return days.map((d) => d[0].toUpperCase() + d[1]).join(', ');
  }
}

class _EmptySchedule extends StatelessWidget {
  final VoidCallback onAdd;
  const _EmptySchedule({required this.onAdd});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Icon(Icons.schedule, size: 64, color: AppColors.textHint),
          const SizedBox(height: 16),
          const Text('No schedules yet',
              style: TextStyle(
                  fontSize: 18,
                  fontWeight: FontWeight.w500,
                  color: AppColors.textSecondary)),
          const SizedBox(height: 8),
          const Text('Add a feeding schedule for your pet',
              style: TextStyle(color: AppColors.textHint)),
          const SizedBox(height: 24),
          ElevatedButton.icon(
            onPressed: onAdd,
            icon: const Icon(Icons.add),
            label: const Text('Add Schedule'),
          ),
        ],
      ),
    );
  }
}

class _ScheduleEditSheet extends StatefulWidget {
  final FeedingSchedule? existing;
  const _ScheduleEditSheet({this.existing});

  @override
  State<_ScheduleEditSheet> createState() => _ScheduleEditSheetState();
}

class _ScheduleEditSheetState extends State<_ScheduleEditSheet> {
  late final TextEditingController _labelCtrl;
  late TimeOfDay _time;
  late double _portionGrams;
  late List<String> _days;
  final _days_options = ['mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'sun'];
  final _days_labels = ['M', 'T', 'W', 'T', 'F', 'S', 'S'];

  @override
  void initState() {
    super.initState();
    final s = widget.existing;
    _labelCtrl = TextEditingController(text: s?.label ?? 'Feeding');
    _time = s != null
        ? TimeOfDay(hour: s.hour, minute: s.minute)
        : const TimeOfDay(hour: 8, minute: 0);
    _portionGrams = (s?.portionGrams ?? AppConfig.defaultPortionGrams).clamp(20.0, 60.0);
    _days = List.from(
        s?.days ?? ['mon', 'tue', 'wed', 'thu', 'fri', 'sat', 'sun']);
  }

  @override
  void dispose() {
    _labelCtrl.dispose();
    super.dispose();
  }

  Future<void> _save() async {
    final provider = context.read<FeedingProvider>();
    final schedule = FeedingSchedule(
      id: widget.existing?.id ?? const Uuid().v4(),
      deviceId: AppConfig.deviceId,
      label: _labelCtrl.text.trim().isEmpty ? 'Feeding' : _labelCtrl.text.trim(),
      hour: _time.hour,
      minute: _time.minute,
      enabled: widget.existing?.enabled ?? true,
      portionGrams: _portionGrams,
      days: _days,
      createdAt: widget.existing?.createdAt ?? DateTime.now(),
    );

    // Close the sheet immediately — no waiting for the network round-trip.
    if (mounted) Navigator.pop(context);

    if (widget.existing != null) {
      await provider.updateSchedule(schedule);
    } else {
      await provider.addSchedule(schedule);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: EdgeInsets.only(
        left: 24,
        right: 24,
        top: 24,
        bottom: MediaQuery.of(context).viewInsets.bottom + 24,
      ),
      decoration: const BoxDecoration(
        color: Colors.white,
        borderRadius: BorderRadius.vertical(top: Radius.circular(24)),
      ),
      child: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Text(
                  widget.existing != null ? 'Edit Schedule' : 'New Schedule',
                  style: const TextStyle(
                      fontSize: 18, fontWeight: FontWeight.bold),
                ),
                const Spacer(),
                IconButton(
                    onPressed: () => Navigator.pop(context),
                    icon: const Icon(Icons.close)),
              ],
            ),
            const SizedBox(height: 16),
            TextFormField(
              controller: _labelCtrl,
              decoration:
                  const InputDecoration(labelText: 'Label (e.g. Morning)'),
            ),
            const SizedBox(height: 16),
            ListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('Feeding Time'),
              subtitle: Text(_time.format(context),
                  style: const TextStyle(
                      fontSize: 24,
                      fontWeight: FontWeight.bold,
                      color: AppColors.primary)),
              trailing: const Icon(Icons.access_time),
              onTap: () async {
                final picked = await showTimePicker(
                    context: context, initialTime: _time);
                if (picked != null) setState(() => _time = picked);
              },
            ),
            const SizedBox(height: 16),
            Text('Portion: ${_portionGrams.toStringAsFixed(0)}g',
                style: const TextStyle(fontWeight: FontWeight.w500)),
            Slider(
              value: _portionGrams,
              min: 20,
              max: 60,
              divisions: 8,
              activeColor: AppColors.primary,
              onChanged: (v) => setState(() => _portionGrams = v),
            ),
            const SizedBox(height: 8),
            const Text('Repeat on:',
                style: TextStyle(fontWeight: FontWeight.w500)),
            const SizedBox(height: 8),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: List.generate(7, (i) {
                final day = _days_options[i];
                final selected = _days.contains(day);
                return GestureDetector(
                  onTap: () => setState(() {
                    if (selected) {
                      if (_days.length > 1) _days.remove(day);
                    } else {
                      _days.add(day);
                    }
                  }),
                  child: Container(
                    width: 36,
                    height: 36,
                    decoration: BoxDecoration(
                      color: selected ? AppColors.primary : AppColors.surfaceVariant,
                      borderRadius: BorderRadius.circular(8),
                    ),
                    alignment: Alignment.center,
                    child: Text(
                      _days_labels[i],
                      style: TextStyle(
                        color: selected ? Colors.white : AppColors.textSecondary,
                        fontWeight: FontWeight.w600,
                        fontSize: 12,
                      ),
                    ),
                  ),
                );
              }),
            ),
            const SizedBox(height: 24),
            ElevatedButton(onPressed: _save, child: const Text('Save Schedule')),
          ],
        ),
      ),
    );
  }
}
