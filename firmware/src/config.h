#pragma once

// ─────────────────────────────────────────────
//  WiFi Credentials
// ─────────────────────────────────────────────
#define WIFI_SSID "jubaru"
#define WIFI_PASSWORD "gigatt02"

// ─────────────────────────────────────────────
//  Firebase Configuration
// ─────────────────────────────────────────────
#define FIREBASE_API_KEY "AIzaSyDgCfVBAeI1rQCPlAa_Y6I3Me_o-GHZAM0"
#define FIREBASE_DATABASE_URL "https://smart-pet-feeder-d0016-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH_EMAIL "device@petfeeder.local"
#define FIREBASE_AUTH_PASSWORD "gigatt02"

// ─────────────────────────────────────────────
//  Device Identity
// ─────────────────────────────────────────────
#define DEVICE_ID "feeder_001"
#define FIRMWARE_VERSION "1.0.0"

// ─────────────────────────────────────────────
//  GPIO Pin Definitions
// ─────────────────────────────────────────────
#define PIN_HX711_DT 4
#define PIN_HX711_SCK 5
#define PIN_ULTRASONIC_TRIG 12
#define PIN_ULTRASONIC_ECHO 14
#define PIN_SERVO 13
#define PIN_WATER_LEVEL 34
#define PIN_LDR 35
#define PIN_HOPPER_LED 27       // GPIO2 is a boot-strapping pin — use GPIO27 instead
#define PIN_RESERVOIR_SENSOR 33 // Analog water level sensor for reservoir (ADC1 — WiFi safe)
#define PIN_RELAY 26
#define PIN_MANUAL_FEED_BTN 25  // Physical feed button — wire to GND, uses internal pull-up

// ─────────────────────────────────────────────
//  Physical Button
// ─────────────────────────────────────────────
// Debounce window — button must be stable for this long before registering.
#define BTN_DEBOUNCE_MS 50UL
// Minimum ms between successive button-triggered feeds (prevents double-trigger).
#define BTN_COOLDOWN_MS 3000UL

// ─────────────────────────────────────────────
//  Load Cell Calibration
// ─────────────────────────────────────────────
#define LOAD_CELL_CALIBRATION_FACTOR 205.0f // 60 g cat food = full bowl
#define LOAD_CELL_TARE_READINGS 20          // Readings averaged for tare
#define LOAD_CELL_SAMPLE_READINGS 10        // Readings averaged per measurement

// ─────────────────────────────────────────────
//  Feeding Logic Thresholds
// ─────────────────────────────────────────────
#define DEFAULT_TARGET_GRAMS 60.0f       // Target food portion in grams
#define BOWL_EMPTY_THRESHOLD_GRAMS 20.0f // Bowl considered empty below this
#define BOWL_FULL_THRESHOLD_GRAMS 60.0f  // 60 g = full bowl (calibrated with cat food)
#define DISPENSE_TIMEOUT_MS 30000UL      // 30s max total dispense time (safety cutoff)
#define PET_WEIGHT_WAIT_MS 15000UL       // Max wait for pet to leave bowl before dispensing

// ─────────────────────────────────────────────
//  Servo Parameters
// ─────────────────────────────────────────────
#define SERVO_CLOSE_ANGLE 0     // Fully closed (0°)
#define SERVO_OPEN_ANGLE 45     // Fully open  (45°) — physical limit
#define SERVO_DISPENSE_ANGLE 25 // Fixed angle used for every dispense pulse

// ─────────────────────────────────────────────
//  Pulse Dispensing
// ─────────────────────────────────────────────
// Each pulse: open 30° → wait PULSE_OPEN_MS → close → wait PULSE_SETTLE_MS → weigh.
// Repeat until bowl reaches target or timeout / max-pulse limit is hit.
#define PULSE_OPEN_MS 500UL   // How long gate stays open per pulse (food flow time)
#define PULSE_SETTLE_MS 600UL // Scale settle time after gate closes before weighing
#define MAX_PULSES 20         // Safety limit on pulse count (~20 × 2 s = 40 s max)

// ─────────────────────────────────────────────
//  Water Bowl Thresholds (pet drinking bowl)
// ─────────────────────────────────────────────
#define WATER_LOW_THRESHOLD 30   // % — trigger refill below this
#define WATER_FULL_THRESHOLD 100 // % — stop pump when bowl reaches this level
// Sensor outputs 0–1.5 V → ESP32 ADC (0–3.3 V, 12-bit) sees max ~1860 counts.
// Adjust these defaults with the calibration menu in Settings → Debug.
#define WATER_EMPTY_ADC 100          // Raw ADC when bowl is empty  (≈0.08 V)
#define WATER_FULL_ADC 1800          // Raw ADC when bowl is full   (≈1.45 V)
#define PUMP_TIMEOUT_MS 15000UL      // 15s max pump run time
#define PUMP_CHECK_INTERVAL_MS 500UL // Check water level every 500ms while pumping

// ─────────────────────────────────────────────
//  Reservoir Thresholds (refill tank)
// ─────────────────────────────────────────────
// Same sensor family — same 0–1.5 V output range.
#define RESERVOIR_EMPTY_ADC 100    // Raw ADC when reservoir is empty (≈0.08 V)
#define RESERVOIR_FULL_ADC 1800    // Raw ADC when reservoir is full  (≈1.45 V)
#define RESERVOIR_LOW_THRESHOLD 10 // % — warn + block pump below this (dry-run protection)
#define RESERVOIR_SAMPLE_COUNT 5   // Readings averaged per measurement
// Sensor is mounted at the MIDDLE of the tank (50 % height).
// Raw ADC can only measure the water above the sensor.
// Display offset: raw 0 % → display 50 %, raw 100 % → display 100 %.
#define RESERVOIR_SENSOR_MIN_PCT 10

// ─────────────────────────────────────────────
//  Hopper Sensor (LDR + LED)
// ─────────────────────────────────────────────
//
//  Choose your physical LED+LDR layout by uncommenting ONE of the two lines:
//
//  Assumes pull-up wiring: VCC → LDR → ADC pin → R(10kΩ) → GND
//    (bright LDR = HIGH voltage, dark LDR = LOW voltage)
//
//  BEAM-BREAK layout  (LED and LDR on OPPOSITE sides of the hopper):
//    Food present → blocks beam  → LDR dark  → LOW  ADC = food OK
//    Food empty   → beam passes  → LDR lit   → HIGH ADC = empty
//
//  REFLECTIVE layout  (LED and LDR on the SAME side, food reflects light back):
//    Food present → reflects beam → LDR lit  → HIGH ADC = food OK
//    Food empty   → no reflection → LDR dark → LOW  ADC = empty
//
#define HOPPER_LAYOUT_BEAM_BREAK // ← beam-break: LED and LDR on opposite sides
// #define HOPPER_LAYOUT_REFLECTIVE   // ← reflective: LED + LDR on same side

// Midpoint between dark (~100) and lit (~1800) for a 0-1.5 V LDR module.
// With BEAM-BREAK: food blocks beam → LDR dark → LOW ADC < threshold = food OK.
//                  beam free (empty) → LDR lit  → HIGH ADC > threshold = EMPTY.
// If serial shows lit values < 900 or dark values > 900, adjust this value.
#define HOPPER_FOOD_PRESENT_ADC 900 // Threshold ADC (calibrate via serial debug)
#define HOPPER_SAMPLE_COUNT 5       // Readings averaged for stability

// LED drive polarity — change if your LED is always on or always off:
//   Active-HIGH (anode → GPIO, cathode → R → GND): HOPPER_LED_ON HIGH
//   Active-LOW  (cathode → GPIO, anode ← R ← VCC): HOPPER_LED_ON LOW
#define HOPPER_LED_ON LOW // active-low wiring (cathode to GPIO)
#define HOPPER_LED_OFF HIGH

// ─────────────────────────────────────────────
//  Ultrasonic Sensor (Pet Detection)
// ─────────────────────────────────────────────
#define PET_DETECTION_DISTANCE_CM 30 // Distance threshold for pet detection
#define ULTRASONIC_TIMEOUT_US 25000  // Timeout for echo pulse (microseconds)

// ─────────────────────────────────────────────
//  Sensor Read Intervals
// ─────────────────────────────────────────────
#define SENSOR_POLL_INTERVAL_MS 10000UL    // Read all sensors every 10s
#define FIREBASE_SYNC_INTERVAL_MS 10000UL  // Push sensor data every 10s
#define HEARTBEAT_INTERVAL_MS 15000UL      // Online heartbeat every 15s
#define AI_ANALYSIS_INTERVAL_MS 600000UL   // AI runs every 10 minutes (fallback timer)
#define SCHEDULE_CHECK_INTERVAL_MS 30000UL // Check feeding schedule every 30s

// ─────────────────────────────────────────────
//  NTP Time Sync
// ─────────────────────────────────────────────
#define NTP_SERVER "pool.ntp.org"
#define NTP_OFFSET_SEC 28800L   // UTC+8 (Philippines) = 8 * 3600
#define NTP_UPDATE_MS 3600000UL // Sync NTP every 1 hour

// ─────────────────────────────────────────────
//  WiFi Reconnection
// ─────────────────────────────────────────────
#define WIFI_RECONNECT_DELAY_MS 5000UL
#define WIFI_MAX_RETRIES 10

// ─────────────────────────────────────────────
//  Relay Logic
// ─────────────────────────────────────────────
#define RELAY_ON LOW // Active LOW relay module
#define RELAY_OFF HIGH

// ─────────────────────────────────────────────
//  Firebase RTDB Paths (relative to device root)
// ─────────────────────────────────────────────
#define FB_PATH_STATUS "/devices/" DEVICE_ID "/status"
#define FB_PATH_SENSORS "/devices/" DEVICE_ID "/sensors"
#define FB_PATH_STATE "/devices/" DEVICE_ID "/state"
#define FB_PATH_COMMANDS "/devices/" DEVICE_ID "/commands"
#define FB_PATH_SETTINGS "/devices/" DEVICE_ID "/settings_cache"
