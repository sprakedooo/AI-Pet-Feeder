class AppConfig {
  static const String deviceId = 'feeder_001';
  static const String rtdbUrl =
      'https://smart-pet-feeder-d0016-default-rtdb.firebaseio.com';

  // Max food bowl capacity in grams (used for percentage display)
  static const double maxBowlGrams = 160.0;

  // Default portion grams (matches BOWL_FULL_THRESHOLD_GRAMS on the firmware)
  static const double defaultPortionGrams = 60.0;
}
