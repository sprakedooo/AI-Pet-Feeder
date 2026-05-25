import 'package:flutter/material.dart';
import '../config/app_colors.dart';

class StatusIndicator extends StatelessWidget {
  final bool online;

  const StatusIndicator({super.key, required this.online});

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          width: 8,
          height: 8,
          decoration: BoxDecoration(
            color: online ? AppColors.success : AppColors.danger,
            shape: BoxShape.circle,
            boxShadow: [
              BoxShadow(
                color: (online ? AppColors.success : AppColors.danger)
                    .withValues(alpha: 0.5),
                blurRadius: 4,
                spreadRadius: 1,
              ),
            ],
          ),
        ),
        const SizedBox(width: 6),
        Text(
          online ? 'Online' : 'Offline',
          style: TextStyle(
            color: online ? AppColors.success : AppColors.danger,
            fontSize: 12,
            fontWeight: FontWeight.w500,
          ),
        ),
      ],
    );
  }
}
