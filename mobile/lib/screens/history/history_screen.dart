import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:intl/intl.dart';
import 'package:fl_chart/fl_chart.dart';
import '../../config/app_colors.dart';
import '../../models/feeding_log.dart';
import '../../providers/feeding_provider.dart';

class HistoryScreen extends StatelessWidget {
  const HistoryScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<FeedingProvider>();

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(title: const Text('Feeding History')),
      body: provider.logsLoading
          ? const Center(child: CircularProgressIndicator())
          : provider.logs.isEmpty
              ? const Center(
                  child: Text('No feeding history yet.',
                      style: TextStyle(color: AppColors.textSecondary)))
              : CustomScrollView(
                  slivers: [
                    SliverPadding(
                      padding: const EdgeInsets.all(16),
                      sliver: SliverList(
                        delegate: SliverChildListDelegate([
                          _WeeklyChart(logs: provider.logs),
                          const SizedBox(height: 16),
                          const Text('Recent Feedings',
                              style: TextStyle(
                                  fontSize: 16,
                                  fontWeight: FontWeight.w600)),
                          const SizedBox(height: 8),
                        ]),
                      ),
                    ),
                    SliverPadding(
                      padding: const EdgeInsets.symmetric(horizontal: 16),
                      sliver: SliverList(
                        delegate: SliverChildBuilderDelegate(
                          (context, i) =>
                              _FeedingLogTile(log: provider.logs[i]),
                          childCount: provider.logs.length,
                        ),
                      ),
                    ),
                    const SliverPadding(
                        padding: EdgeInsets.only(bottom: 80)),
                  ],
                ),
    );
  }
}

class _WeeklyChart extends StatelessWidget {
  final List<FeedingLog> logs;

  const _WeeklyChart({required this.logs});

  @override
  Widget build(BuildContext context) {
    // Build last 7 days totals
    final now = DateTime.now();
    final dailyTotals = List<double>.filled(7, 0);
    for (final log in logs) {
      if (!log.isSuccess) continue;
      final diff = now.difference(log.timestamp).inDays;
      if (diff < 7) dailyTotals[6 - diff] += log.dispensedGrams;
    }

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('7-Day Food Intake',
                style: TextStyle(fontWeight: FontWeight.w600)),
            const SizedBox(height: 4),
            Text('${dailyTotals.fold(0.0, (a, b) => a + b).toStringAsFixed(0)}g this week',
                style: const TextStyle(
                    color: AppColors.textSecondary, fontSize: 12)),
            const SizedBox(height: 16),
            SizedBox(
              height: 120,
              child: BarChart(
                BarChartData(
                  alignment: BarChartAlignment.spaceAround,
                  maxY: dailyTotals.reduce((a, b) => a > b ? a : b) + 20,
                  barTouchData: BarTouchData(enabled: false),
                  titlesData: FlTitlesData(
                    bottomTitles: AxisTitles(
                      sideTitles: SideTitles(
                        showTitles: true,
                        getTitlesWidget: (val, _) {
                          final day = now.subtract(
                              Duration(days: 6 - val.toInt()));
                          return Text(
                              DateFormat('E').format(day).substring(0, 1),
                              style: const TextStyle(
                                  fontSize: 10,
                                  color: AppColors.textSecondary));
                        },
                        reservedSize: 20,
                      ),
                    ),
                    leftTitles: const AxisTitles(
                        sideTitles: SideTitles(showTitles: false)),
                    topTitles: const AxisTitles(
                        sideTitles: SideTitles(showTitles: false)),
                    rightTitles: const AxisTitles(
                        sideTitles: SideTitles(showTitles: false)),
                  ),
                  gridData: const FlGridData(show: false),
                  borderData: FlBorderData(show: false),
                  barGroups: List.generate(
                    7,
                    (i) => BarChartGroupData(
                      x: i,
                      barRods: [
                        BarChartRodData(
                          toY: dailyTotals[i],
                          color: AppColors.primary,
                          width: 16,
                          borderRadius: const BorderRadius.vertical(
                              top: Radius.circular(4)),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _FeedingLogTile extends StatelessWidget {
  final FeedingLog log;

  const _FeedingLogTile({required this.log});

  @override
  Widget build(BuildContext context) {
    final color = log.isSuccess
        ? AppColors.success
        : log.isSkipped
            ? AppColors.warning
            : AppColors.danger;

    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        leading: Container(
          width: 40,
          height: 40,
          decoration: BoxDecoration(
            color: color.withOpacity(0.1),
            borderRadius: BorderRadius.circular(10),
          ),
          child: Icon(
            log.isSuccess
                ? Icons.check_circle_outline
                : log.isSkipped
                    ? Icons.skip_next
                    : Icons.error_outline,
            color: color,
          ),
        ),
        title: Text(log.statusLabel,
            style: const TextStyle(fontWeight: FontWeight.w500, fontSize: 14)),
        subtitle: Text(
          log.isSuccess
              ? '${log.dispensedGrams.toStringAsFixed(1)}g dispensed'
              : log.triggerType == 'manual'
                  ? 'Manual trigger'
                  : 'Scheduled',
          style: const TextStyle(fontSize: 12, color: AppColors.textSecondary),
        ),
        trailing: Text(
          DateFormat('MMM d\nh:mm a').format(log.timestamp),
          textAlign: TextAlign.right,
          style: const TextStyle(
              fontSize: 11, color: AppColors.textSecondary),
        ),
      ),
    );
  }
}
