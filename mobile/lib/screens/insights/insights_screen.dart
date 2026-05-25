import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:intl/intl.dart';
import '../../config/app_colors.dart';
import '../../models/ai_insight.dart';
import '../../providers/ai_provider.dart';
import '../../providers/feeding_provider.dart';

class InsightsScreen extends StatelessWidget {
  const InsightsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final aiProvider = context.watch<AIProvider>();
    final feedingProvider = context.watch<FeedingProvider>();

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: const Text('AI Insights'),
        actions: [
          if (aiProvider.unreadCount > 0)
            TextButton(
              onPressed: () {
                for (final i in aiProvider.insights) {
                  if (!i.isRead) aiProvider.markRead(i.id);
                }
              },
              child: const Text('Mark all read'),
            ),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            // Analytics summary cards
            _AnalyticsSummary(
              todayGrams: feedingProvider.todayTotalGrams,
              feedingCount: feedingProvider.todayFeedingCount,
            ),
            const SizedBox(height: 24),

            // AI insights list
            const Text('AI Recommendations',
                style: TextStyle(
                    fontSize: 16, fontWeight: FontWeight.w600)),
            const SizedBox(height: 12),
            aiProvider.loading
                ? const Center(child: CircularProgressIndicator())
                : aiProvider.insights.isEmpty
                    ? _EmptyInsights()
                    : Column(
                        children: aiProvider.insights
                            .map((i) => _InsightCard(
                                insight: i,
                                onRead: () => aiProvider.markRead(i.id)))
                            .toList(),
                      ),
          ],
        ),
      ),
    );
  }
}

class _AnalyticsSummary extends StatelessWidget {
  final double todayGrams;
  final int feedingCount;

  const _AnalyticsSummary(
      {required this.todayGrams, required this.feedingCount});

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        const Text('Today\'s Analytics',
            style: TextStyle(
                fontSize: 16, fontWeight: FontWeight.w600)),
        const SizedBox(height: 12),
        Row(
          children: [
            Expanded(
              child: _AnalyticsCard(
                title: 'Food Consumed',
                value: '${todayGrams.toStringAsFixed(0)}g',
                subtitle: 'today',
                icon: Icons.food_bank,
                color: AppColors.primary,
              ),
            ),
            const SizedBox(width: 12),
            Expanded(
              child: _AnalyticsCard(
                title: 'Feedings',
                value: '$feedingCount',
                subtitle: 'today',
                icon: Icons.restaurant,
                color: AppColors.accent,
              ),
            ),
          ],
        ),
      ],
    );
  }
}

class _AnalyticsCard extends StatelessWidget {
  final String title;
  final String value;
  final String subtitle;
  final IconData icon;
  final Color color;

  const _AnalyticsCard({
    required this.title,
    required this.value,
    required this.subtitle,
    required this.icon,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Icon(icon, color: color, size: 24),
            const SizedBox(height: 8),
            Text(value,
                style: TextStyle(
                    fontSize: 28,
                    fontWeight: FontWeight.bold,
                    color: color)),
            Text(title,
                style: const TextStyle(
                    fontSize: 12,
                    fontWeight: FontWeight.w500,
                    color: AppColors.textPrimary)),
            Text(subtitle,
                style: const TextStyle(
                    fontSize: 11, color: AppColors.textSecondary)),
          ],
        ),
      ),
    );
  }
}

class _InsightCard extends StatelessWidget {
  final AIInsight insight;
  final VoidCallback onRead;

  const _InsightCard({required this.insight, required this.onRead});

  Color get _severityColor {
    switch (insight.severity) {
      case 5:
      case 4:
        return AppColors.danger;
      case 3:
        return AppColors.warning;
      default:
        return AppColors.info;
    }
  }

  IconData get _typeIcon {
    switch (insight.type) {
      case 'appetite_warning':
      case 'low_appetite_alert':
        return Icons.warning_amber_rounded;
      case 'dehydration_risk':
        return Icons.water_drop_outlined;
      case 'refill_prediction':
        return Icons.inventory_2_outlined;
      case 'overfeeding_risk':
        return Icons.no_food_outlined;
      default:
        return Icons.lightbulb_outlined;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 10),
      child: InkWell(
        onTap: !insight.isRead ? onRead : null,
        borderRadius: BorderRadius.circular(16),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Container(
                padding: const EdgeInsets.all(10),
                decoration: BoxDecoration(
                  color: _severityColor.withValues(alpha: 0.1),
                  borderRadius: BorderRadius.circular(10),
                ),
                child: Icon(_typeIcon, color: _severityColor, size: 22),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Row(
                      children: [
                        Expanded(
                          child: Text(insight.title,
                              style: const TextStyle(
                                  fontWeight: FontWeight.w600,
                                  fontSize: 14)),
                        ),
                        if (!insight.isRead)
                          Container(
                            width: 8,
                            height: 8,
                            decoration: BoxDecoration(
                              color: _severityColor,
                              shape: BoxShape.circle,
                            ),
                          ),
                      ],
                    ),
                    const SizedBox(height: 4),
                    Text(insight.message,
                        style: const TextStyle(
                            fontSize: 13,
                            color: AppColors.textSecondary,
                            height: 1.4)),
                    const SizedBox(height: 8),
                    Row(
                      children: [
                        Container(
                          padding: const EdgeInsets.symmetric(
                              horizontal: 8, vertical: 2),
                          decoration: BoxDecoration(
                            color: _severityColor.withValues(alpha: 0.1),
                            borderRadius: BorderRadius.circular(4),
                          ),
                          child: Text(insight.severityLabel,
                              style: TextStyle(
                                  fontSize: 10,
                                  color: _severityColor,
                                  fontWeight: FontWeight.w600)),
                        ),
                        const Spacer(),
                        Text(
                          DateFormat('MMM d, h:mm a')
                              .format(insight.generatedAt),
                          style: const TextStyle(
                              fontSize: 11,
                              color: AppColors.textHint),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _EmptyInsights extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          children: const [
            Icon(Icons.lightbulb_outline, size: 48, color: AppColors.textHint),
            SizedBox(height: 12),
            Text('No insights yet',
                style: TextStyle(
                    fontSize: 16,
                    fontWeight: FontWeight.w500,
                    color: AppColors.textSecondary)),
            SizedBox(height: 4),
            Text(
              'AI will analyze your pet\'s feeding patterns\nand generate insights after a few feedings.',
              textAlign: TextAlign.center,
              style:
                  TextStyle(color: AppColors.textHint, fontSize: 13),
            ),
          ],
        ),
      ),
    );
  }
}
