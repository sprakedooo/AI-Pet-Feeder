# Setup Guide

## Prerequisites

### Tools Required
- [ ] VS Code with PlatformIO extension
- [ ] Flutter SDK 3.x+
- [ ] Firebase CLI (`npm install -g firebase-tools`)
- [ ] Git
- [ ] Arduino driver for ESP32 (CP2102 or CH340)

### Accounts Required
- [ ] Firebase account (free Spark plan works)
- [ ] Google account (for Firebase Auth)

---

## Step 1: Firebase Project Setup

### 1.1 Create Firebase Project

1. Go to https://console.firebase.google.com
2. Click "Add project"
3. Name it: `smart-pet-feeder`
4. Disable Google Analytics (optional)
5. Click "Create project"

### 1.2 Enable Authentication

1. Go to **Authentication → Sign-in method**
2. Enable **Email/Password**
3. (Optional) Enable **Google Sign-In**

### 1.3 Enable Realtime Database

1. Go to **Realtime Database → Create database**
2. Choose your region (e.g., `us-central1`)
3. Start in **test mode** (we'll add rules later)
4. Copy your database URL: `https://your-project-id-default-rtdb.firebaseio.com`

### 1.4 Enable Firestore

1. Go to **Firestore Database → Create database**
2. Start in **test mode**
3. Choose your region (match RTDB region)

### 1.5 Enable Cloud Messaging (FCM)

1. Go to **Project Settings → Cloud Messaging**
2. Copy your **Server Key** (for backend use)
3. Copy your **Sender ID**

### 1.6 Get Firebase Config

For **Flutter app** (Android):
1. Go to **Project Settings → General**
2. Click **Add app → Android**
3. Enter package name: `com.yourname.petfeeder`
4. Download `google-services.json`
5. Place in `mobile/android/app/`

For **Flutter app** (iOS, optional):
1. Click **Add app → iOS**
2. Download `GoogleService-Info.plist`
3. Place in `mobile/ios/Runner/`

For **ESP32 firmware**:
1. Go to **Project Settings → Service Accounts**
2. Generate **new private key** → download JSON
3. Extract the fields needed for `config.h` (API key, database URL, auth domain)

### 1.7 Get Web API Key

1. **Project Settings → General → Your apps → Web API key**
2. Copy this key for `config.h`

---

## Step 2: Firmware Setup (ESP32)

### 2.1 Install PlatformIO

1. Install VS Code
2. Install PlatformIO IDE extension
3. Restart VS Code

### 2.2 Open Firmware Project

```bash
cd "AI PET FEEDER/firmware"
code .
```

PlatformIO will auto-detect `platformio.ini` and install dependencies.

### 2.3 Configure firmware/src/config.h

Edit `firmware/src/config.h` and fill in:

```cpp
// WiFi
#define WIFI_SSID       "YourWiFiName"
#define WIFI_PASSWORD   "YourWiFiPassword"

// Firebase
#define FIREBASE_API_KEY      "your-web-api-key"
#define FIREBASE_DATABASE_URL "https://your-project-id-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH_EMAIL   "device@yourproject.com"
#define FIREBASE_AUTH_PASSWORD "devicepassword123"

// Device
#define DEVICE_ID       "feeder_001"
```

### 2.4 Create Firebase Device Account

1. Go to **Firebase Auth → Users → Add user**
2. Email: `device@yourproject.com`
3. Password: a strong password
4. This account is only used by the ESP32 firmware

### 2.5 Upload Firmware

1. Connect ESP32 via USB
2. In PlatformIO, click **Upload** (→ button)
3. Open **Serial Monitor** (115200 baud)
4. Verify connection messages appear

### 2.6 Calibrate Load Cell

After first upload:
1. Open Serial Monitor
2. Place nothing on the scale → note raw value (tare)
3. Place known weight (e.g., 100g) → note raw value
4. Calculate `LOAD_CELL_CALIBRATION_FACTOR`:
   ```
   factor = (raw_with_weight - raw_empty) / known_weight_grams
   ```
5. Update `LOAD_CELL_CALIBRATION_FACTOR` in `config.h`
6. Re-upload firmware

---

## Step 3: Mobile App Setup (Flutter)

### 3.1 Install Flutter

Follow https://flutter.dev/docs/get-started/install for your OS.

Verify:
```bash
flutter doctor
```
All checks should pass (Android toolchain required; iOS optional).

### 3.2 Open Mobile Project

```bash
cd "AI PET FEEDER/mobile"
flutter pub get
```

### 3.3 Place Firebase Config Files

- **Android**: Copy `google-services.json` → `android/app/google-services.json`
- **iOS**: Copy `GoogleService-Info.plist` → `ios/Runner/GoogleService-Info.plist`

### 3.4 Configure App

Edit `lib/config/app_config.dart`:

```dart
class AppConfig {
  static const String deviceId = 'feeder_001';
  static const String rtdbUrl = 'https://your-project-id-default-rtdb.firebaseio.com';
}
```

### 3.5 Run the App

```bash
# Android
flutter run

# iOS (Mac only)
flutter run -d ios

# Web (for testing)
flutter run -d chrome
```

### 3.6 First App Launch

1. Open the app
2. Tap **Register** and create your user account
3. The app will auto-link to `DEVICE_ID` in config
4. Dashboard will show live data once ESP32 is online

---

## Step 4: Firebase Security Rules

### 4.1 Deploy Firestore Rules

```bash
cd "AI PET FEEDER/backend"
firebase login
firebase init firestore
firebase deploy --only firestore:rules
```

### 4.2 Deploy RTDB Rules

1. Go to **Realtime Database → Rules**
2. Paste:

```json
{
  "rules": {
    "devices": {
      "$deviceId": {
        ".read": "auth != null",
        ".write": "auth != null"
      }
    }
  }
}
```

---

## Step 5: Hardware Assembly

Refer to [WIRING_GUIDE.md](WIRING_GUIDE.md) for detailed wiring.

Quick checklist:
- [ ] Load cell mounted under bowl platform
- [ ] HX711 wired to GPIO 4 (DT) and GPIO 5 (SCK)
- [ ] Water sensor wired to GPIO 34
- [ ] Ultrasonic wired to GPIO 12 (TRIG) and GPIO 14 (ECHO)
- [ ] LDR wired to GPIO 35, LED to GPIO 2
- [ ] Float sensor wired to GPIO 27 with pull-up
- [ ] Servo wired to GPIO 13 (external 5V power)
- [ ] Relay wired to GPIO 26 (IN)
- [ ] Pump connected through relay NO/COM terminals
- [ ] All grounds connected together

---

## Step 6: System Test

### 6.1 Sensor Test
Open Serial Monitor and verify:
```
[SENSOR] Bowl weight: 0.0g
[SENSOR] Water level: 85%
[SENSOR] Hopper: OK
[SENSOR] Reservoir: OK
[SENSOR] Pet detected: false (distance: 150cm)
```

### 6.2 Actuator Test
In the app, tap **Manual Feed** → servo should open and close.
Tap **Manual Water** → pump should run for 3 seconds.

### 6.3 Schedule Test
Add a schedule 2 minutes from now → verify auto-dispensing triggers.

### 6.4 Notification Test
Empty the hopper (remove food) → app should receive "Hopper Empty" notification.

---

## Step 7: Calibration

### Load Cell Calibration
```
1. Power on ESP32
2. Ensure bowl is empty and on scale
3. Wait for "Tare complete" in serial log
4. Place a known weight (e.g., 100g)
5. Check serial output for calibration factor
6. Adjust LOAD_CELL_CALIBRATION_FACTOR in config.h
7. Repeat until reading matches actual weight within ±2g
```

### Water Sensor Calibration
```
1. Read ADC value when bowl is empty → WATER_EMPTY_ADC
2. Read ADC value when bowl is full  → WATER_FULL_ADC
3. Update both values in config.h
```

### LDR Calibration
```
1. Fill hopper with food → read LDR ADC → set HOPPER_FOOD_THRESHOLD_HIGH
2. Empty hopper        → read LDR ADC → set HOPPER_FOOD_THRESHOLD_LOW
3. Threshold = midpoint between both values
```

---

## Troubleshooting

| Problem | Likely Cause | Fix |
|---|---|---|
| WiFi not connecting | Wrong SSID/password | Check config.h |
| Firebase auth failed | Wrong API key or email | Re-check Firebase console |
| Load cell reads 0 | Wiring issue or wrong calibration | Check HX711 wiring |
| Servo not moving | Power issue | Use external 5V supply |
| Pump not turning on | Relay wiring or GPIO logic | Check NO vs NC, active LOW |
| App shows offline | ESP32 not sending heartbeat | Check Firebase rules |
| Notifications not arriving | FCM token not registered | Re-login in app |
| Water sensor noisy | ADC interference | Add 100nF cap between Signal and GND |
