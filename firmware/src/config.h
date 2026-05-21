#pragma once

// ─────────────────────────────────────────────
//  WiFi Credentials
// ─────────────────────────────────────────────
#define WIFI_SSID           "YourWiFiSSID"
#define WIFI_PASSWORD       "YourWiFiPassword"

// ─────────────────────────────────────────────
//  Firebase Configuration
// ─────────────────────────────────────────────
#define FIREBASE_API_KEY         "your-firebase-web-api-key"
#define FIREBASE_DATABASE_URL    "https://your-project-id-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH_EMAIL      "device@yourproject.com"
#define FIREBASE_AUTH_PASSWORD   "DeviceStrongPassword123"

// ─────────────────────────────────────────────
//  Device Identity
// ─────────────────────────────────────────────
#define DEVICE_ID           "feeder_001"
#define FIRMWARE_VERSION    "1.0.0"

// ─────────────────────────────────────────────
//  GPIO Pin Definitions
// ─────────────────────────────────────────────
#define PIN_HX711_DT          4
#define PIN_HX711_SCK         5
#define PIN_ULTRASONIC_TRIG   12
#define PIN_ULTRASONIC_ECHO   14
#define PIN_SERVO             13
#define PIN_WATER_LEVEL       34
#define PIN_LDR               35
#define PIN_HOPPER_LED        2
#define PIN_FLOAT_SENSOR      27
#define PIN_RELAY             26

// ─────────────────────────────────────────────
//  Load Cell Calibration
// ─────────────────────────────────────────────
#define LOAD_CELL_CALIBRATION_FACTOR   420.0f   // Adjust after calibration
#define LOAD_CELL_TARE_READINGS        20        // Readings averaged for tare
#define LOAD_CELL_SAMPLE_READINGS      10        // Readings averaged per measurement

// ─────────────────────────────────────────────
//  Feeding Logic Thresholds
// ─────────────────────────────────────────────
#define DEFAULT_TARGET_GRAMS          80.0f     // Target food portion in grams
#define BOWL_EMPTY_THRESHOLD_GRAMS    20.0f     // Bowl considered empty below this
#define BOWL_FULL_THRESHOLD_GRAMS     60.0f     // Bowl considered full above this
#define MAX_DISPENSE_RETRIES          3
#define DISPENSE_TIMEOUT_MS           30000UL   // 30s max dispense duration
#define DISPENSE_CYCLE_DELAY_MS       2000UL    // Delay between dispense cycles
#define FEEDING_VERIFY_DELAY_MS       3000UL    // Wait after dispensing to weigh

// ─────────────────────────────────────────────
//  Servo Parameters
// ─────────────────────────────────────────────
#define SERVO_CLOSE_ANGLE      0
#define SERVO_OPEN_ANGLE       90
#define SERVO_SHAKE_ANGLE_A    80
#define SERVO_SHAKE_ANGLE_B    100
#define SERVO_SHAKE_CYCLES     3
#define SERVO_SHAKE_DELAY_MS   150
#define SERVO_OPEN_DELAY_MS    500
#define SERVO_CLOSE_SPEED_MS   20    // ms per degree when closing slowly

// ─────────────────────────────────────────────
//  Water System Thresholds
// ─────────────────────────────────────────────
#define WATER_LOW_THRESHOLD       30     // % — trigger refill below this
#define WATER_FULL_THRESHOLD      85     // % — stop pump at this level
#define WATER_EMPTY_ADC           200    // Raw ADC when bowl is empty
#define WATER_FULL_ADC            3800   // Raw ADC when bowl is full
#define PUMP_TIMEOUT_MS           15000UL  // 15s max pump run time
#define PUMP_CHECK_INTERVAL_MS    500UL    // Check water level every 500ms while pumping

// ─────────────────────────────────────────────
//  Hopper Sensor (LDR + LED)
// ─────────────────────────────────────────────
//
//  Choose your physical LED+LDR layout by uncommenting ONE of the two lines:
//
//  BEAM-BREAK layout  (LED and LDR on OPPOSITE sides of the hopper):
//    Food present → blocks beam  → LDR dark  → HIGH ADC = food OK
//    Food empty   → beam passes  → LDR lit   → LOW  ADC = empty
//
//  REFLECTIVE layout  (LED and LDR on the SAME side, food reflects light):
//    Food present → reflects beam → LDR lit  → LOW  ADC = food OK
//    Food empty   → no reflection → LDR dark → HIGH ADC = empty
//
#define HOPPER_LAYOUT_BEAM_BREAK      // ← uncomment this for beam-break
// #define HOPPER_LAYOUT_REFLECTIVE   // ← uncomment this for reflective

#define HOPPER_FOOD_PRESENT_ADC    2000  // Threshold ADC value (tune after testing)
#define HOPPER_SAMPLE_COUNT        5     // Readings averaged for stability

// ─────────────────────────────────────────────
//  Ultrasonic Sensor (Pet Detection)
// ─────────────────────────────────────────────
#define PET_DETECTION_DISTANCE_CM  30    // Distance threshold for pet detection
#define ULTRASONIC_TIMEOUT_US      25000 // Timeout for echo pulse (microseconds)

// ─────────────────────────────────────────────
//  Sensor Read Intervals
// ─────────────────────────────────────────────
#define SENSOR_POLL_INTERVAL_MS    10000UL   // Read all sensors every 10s
#define FIREBASE_SYNC_INTERVAL_MS  10000UL   // Push sensor data every 10s
#define HEARTBEAT_INTERVAL_MS      60000UL   // Online heartbeat every 60s
#define AI_ANALYSIS_INTERVAL_MS    1800000UL // AI runs every 30 minutes
#define SCHEDULE_CHECK_INTERVAL_MS 30000UL   // Check feeding schedule every 30s

// ─────────────────────────────────────────────
//  NTP Time Sync
// ─────────────────────────────────────────────
#define NTP_SERVER        "pool.ntp.org"
#define NTP_OFFSET_SEC    28800L       // UTC+8 (Philippines) = 8 * 3600
#define NTP_UPDATE_MS     3600000UL   // Sync NTP every 1 hour

// ─────────────────────────────────────────────
//  WiFi Reconnection
// ─────────────────────────────────────────────
#define WIFI_RECONNECT_DELAY_MS   5000UL
#define WIFI_MAX_RETRIES          10

// ─────────────────────────────────────────────
//  Relay Logic
// ─────────────────────────────────────────────
#define RELAY_ON    LOW    // Active LOW relay module
#define RELAY_OFF   HIGH

// ─────────────────────────────────────────────
//  Firebase RTDB Paths (relative to device root)
// ─────────────────────────────────────────────
#define FB_PATH_STATUS      "/devices/" DEVICE_ID "/status"
#define FB_PATH_SENSORS     "/devices/" DEVICE_ID "/sensors"
#define FB_PATH_STATE       "/devices/" DEVICE_ID "/state"
#define FB_PATH_COMMANDS    "/devices/" DEVICE_ID "/commands"
#define FB_PATH_SETTINGS    "/devices/" DEVICE_ID "/settings_cache"
