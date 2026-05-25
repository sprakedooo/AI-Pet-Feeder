# AI Pet Feeder — Complete Setup Guide

Follow these steps **in order**. Each section tells you what you need and exactly what to do.

---

## Prerequisites

Install these tools on your computer before starting:

| Tool | Purpose | Download |
|---|---|---|
| **Flutter SDK** (3.x) | Build mobile app | https://flutter.dev/docs/get-started/install |
| **Android Studio** | Android build tools + emulator | https://developer.android.com/studio |
| **PlatformIO** (VS Code extension) | Flash ESP32 firmware | https://platformio.org/install/ide?install=vscode |
| **Node.js** (v18+) | Run Firebase CLI | https://nodejs.org |
| **Firebase CLI** | Deploy backend | `npm install -g firebase-tools` |
| **flutterfire CLI** | Generate Firebase config | `dart pub global activate flutterfire_cli` |

---

## Step 1 — Create Your Firebase Project

1. Go to **https://console.firebase.google.com**
2. Click **Add project** → name it (e.g. `ai-pet-feeder`) → Continue
3. Disable Google Analytics (optional) → **Create project**

### Enable required services:

#### Authentication
1. Left sidebar → **Build → Authentication → Get started**
2. Click **Sign-in method** tab
3. Enable **Email/Password** → Save

#### Firestore Database
1. Left sidebar → **Build → Firestore Database → Create database**
2. Choose **Start in production mode** → Next
3. Select a location closest to you → **Enable**

#### Realtime Database
1. Left sidebar → **Build → Realtime Database → Create database**
2. Choose a location → **Start in locked mode** → **Enable**
3. After creation, copy the **Database URL** — looks like:
   `https://your-project-id-default-rtdb.firebaseio.com`
   You will need this in Step 4.

#### Cloud Messaging
1. Left sidebar → **Project Settings** (gear icon)
2. Click **Cloud Messaging** tab — it is enabled by default, nothing to configure

#### Cloud Functions
1. Left sidebar → **Build → Functions → Get started**
2. Follow prompts — requires upgrading to **Blaze (pay-as-you-go)** plan
   *(The free tier is enough for personal use — you will not be charged unless traffic is very high)*

---

## Step 2 — Create the ESP32 Service Account

The ESP32 needs its own Firebase user account to authenticate.

1. In Firebase Console → **Authentication → Users tab**
2. Click **Add user**
3. Enter:
   - **Email:** `device@petfeeder.local` (or any email you want)
   - **Password:** choose a strong password
4. Click **Add user**
5. Note down the email and password — you will paste them into `config.h` in Step 4

---

## Step 3 — Configure the Flutter App

### 3a. Generate firebase_options.dart

Open a terminal, navigate to the mobile folder:

```bash
cd "C:\Users\jubil\AI PET FEEDER\mobile"
```

Log in to Firebase and run flutterfire configure:

```bash
firebase login
flutterfire configure
```

When prompted:
- Select your Firebase project (the one you created in Step 1)
- Select **Android** (and iOS if you want iOS support)
- Accept the default app ID or enter `com.example.smart_pet_feeder`

This generates `lib/firebase_options.dart` automatically.

### 3b. Update main.dart to use the generated options

Open `mobile/lib/main.dart` and update the Firebase init line:

```dart
import 'firebase_options.dart';           // add this import

// change this line:
await Firebase.initializeApp();

// to this:
await Firebase.initializeApp(
  options: DefaultFirebaseOptions.currentPlatform,
);
```

### 3c. Update the RTDB URL in app_config.dart

Open `mobile/lib/config/app_config.dart` and replace the placeholder:

```dart
static const String rtdbUrl = 'https://YOUR-PROJECT-ID-default-rtdb.firebaseio.com';
//                                      ↑ paste your actual Realtime Database URL here
```

### 3d. Place google-services.json

1. In Firebase Console → **Project Settings → Your apps**
2. If no Android app exists, click **Add app → Android**
   - Package name: `com.example.smart_pet_feeder`
   - Register app
3. Download **google-services.json**
4. Place it at:
   ```
   mobile/android/app/google-services.json
   ```

### 3e. Create the asset folders

Flutter will fail to build if these declared asset folders do not exist:

```bash
mkdir "C:\Users\jubil\AI PET FEEDER\mobile\assets\images"
mkdir "C:\Users\jubil\AI PET FEEDER\mobile\assets\animations"
```

Place at least one placeholder file in each folder (e.g. a blank `.gitkeep` file),
or remove the asset entries from `pubspec.yaml` if you have no images yet.

### 3f. Generate the Android Gradle wrapper

Run this once inside the mobile folder to generate the missing Gradle wrapper files:

```bash
cd "C:\Users\jubil\AI PET FEEDER\mobile"
flutter create . --project-name smart_pet_feeder
```

This only generates missing files — it will not overwrite your existing code.

---

## Step 4 — Configure the ESP32 Firmware

Open `firmware/src/config.h` and fill in every placeholder:

```cpp
// ── WiFi ──────────────────────────────────────────────────────────
#define WIFI_SSID           "YourActualWiFiName"
#define WIFI_PASSWORD       "YourActualWiFiPassword"

// ── Firebase ──────────────────────────────────────────────────────
// Web API Key: Firebase Console → Project Settings → General → Web API Key
#define FIREBASE_API_KEY         "AIzaSy..."

// Realtime Database URL (copied in Step 1)
#define FIREBASE_DATABASE_URL    "https://your-project-id-default-rtdb.firebaseio.com"

// Service account created in Step 2
#define FIREBASE_AUTH_EMAIL      "device@petfeeder.local"
#define FIREBASE_AUTH_PASSWORD   "YourStrongPassword"
```

---

## Step 5 — Deploy the Firebase Backend

```bash
cd "C:\Users\jubil\AI PET FEEDER\backend"
firebase login
firebase use --add    # select your project when prompted
```

Install Cloud Function dependencies:

```bash
cd functions
npm install
cd ..
```

Deploy everything at once:

```bash
firebase deploy --only firestore:rules,firestore:indexes,database,functions
```

Expected output:
```
✔  firestore: released rules
✔  firestore: deployed indexes
✔  database: released rules
✔  functions: deployed 4 functions
```

---

## Step 6 — Sensor Calibration

You must calibrate 5 values before readings are accurate.
Do this **after wiring everything** and **after flashing the firmware**.

Open PlatformIO Serial Monitor at **115200 baud** to see live readings.

---

### 6a. Load Cell — Food Bowl Weight (GPIO 4/5)

1. Place the **empty bowl** on the load cell and power on the ESP32
2. The firmware tares automatically on boot
3. Place a **known weight** on the bowl (e.g. a 100g coin stack or kitchen weights)
4. Watch Serial Monitor for the raw reading
5. Calculate:
   ```
   CALIBRATION_FACTOR = raw_reading / known_weight_grams
   ```
6. Open `config.h` and update:
   ```cpp
   #define LOAD_CELL_CALIBRATION_FACTOR   420.0f   // ← replace with your value
   ```
7. Reflash and confirm the reading matches your known weight

---

### 6b. Water Bowl Level Sensor (GPIO 34)

1. With the water bowl **completely empty**, note the ADC value in Serial Monitor
   → this is `WATER_EMPTY_ADC`
2. Fill the bowl to the **maximum fill line**, note the ADC value
   → this is `WATER_FULL_ADC`
3. Update `config.h`:
   ```cpp
   #define WATER_EMPTY_ADC    200    // ← replace with your empty reading
   #define WATER_FULL_ADC    3800    // ← replace with your full reading
   ```

---

### 6c. Reservoir Level Sensor (GPIO 33)

Same process as the bowl sensor, but for the refill tank:

1. **Empty reservoir** → note ADC value → `RESERVOIR_EMPTY_ADC`
2. **Full reservoir** → note ADC value → `RESERVOIR_FULL_ADC`
3. Update `config.h`:
   ```cpp
   #define RESERVOIR_EMPTY_ADC    200    // ← replace with your empty reading
   #define RESERVOIR_FULL_ADC    3800    // ← replace with your full reading
   ```

> **Important:** Once calibrated, the pump will automatically refuse to start
> if the reservoir drops below 20% — protecting it from dry running.

---

### 6d. Hopper Food Sensor — LDR + LED Beam Break (GPIO 35 / GPIO 2)

1. **Fill the hopper with food** (beam is blocked) → note ADC reading → `full_reading`
2. **Empty the hopper completely** (beam passes through) → note ADC reading → `empty_reading`
3. Set threshold to halfway between the two:
   ```
   HOPPER_FOOD_PRESENT_ADC = (full_reading + empty_reading) / 2
   ```
   Example: full = 3100, empty = 250 → threshold = 1675
4. Update `config.h`:
   ```cpp
   #define HOPPER_FOOD_PRESENT_ADC    2000    // ← replace with your calculated value
   ```

---

### 6e. Pet Detection — Ultrasonic Sensor (GPIO 12/14)

1. Mount the sensor **above the feeding bowl pointing downward**
2. With **no pet present**, note the distance reading in Serial Monitor (e.g. 42 cm)
3. Set threshold to ~70% of that distance:
   ```
   PET_DETECTION_DISTANCE_CM = mounting_height_cm × 0.70
   ```
   Example: sensor at 40 cm → threshold = 28 cm
4. Update `config.h`:
   ```cpp
   #define PET_DETECTION_DISTANCE_CM  30    // ← replace with your calculated value
   ```

---

## Step 7 — Flash the ESP32

1. Open the `firmware/` folder in **VS Code with PlatformIO**
2. Connect the ESP32 via USB
3. Click the **Upload** button (→) in the PlatformIO toolbar
4. Wait for upload to complete
5. Open Serial Monitor at **115200 baud** — you should see:

```
[WiFi] Connecting to YourWiFiName...
[WiFi] Connected. IP: 192.168.x.x
[Firebase] Connected.
[SensorManager] All sensors initialized.
[Sensors] Bowl:0.0g(0%) Water:45% Reservoir:78% Hopper:ok Pet:no(112.0cm)
[StateMachine] State: IDLE
```

If WiFi does not connect within 30 seconds, the ESP32 will restart automatically.

---

## Step 8 — Build and Install the Flutter App

```bash
cd "C:\Users\jubil\AI PET FEEDER\mobile"
flutter pub get
flutter run                  # run directly on connected phone
```

Or build a release APK:

```bash
flutter build apk --release
```

The APK will be at:
```
mobile/build/app/outputs/flutter-apk/app-release.apk
```

Transfer it to your Android phone and install it.
*(Enable "Install from unknown sources" in Android settings if prompted.)*

---

## Step 9 — First Run Checklist

Verify each item after everything is running:

- [ ] App opens without crash
- [ ] Login / Register works
- [ ] Dashboard shows live sensor readings (not all zeros)
- [ ] Sensor values update every ~10 seconds
- [ ] **Feed Now** button triggers the servo and load cell detects a weight change
- [ ] Servo opens to **45°** and closes back to **0°** cleanly
- [ ] **Water Now** button starts the pump and stops when bowl reaches 85%
- [ ] Pump does NOT start when reservoir is below 20%
- [ ] Feeding schedule saves in app and ESP32 picks it up within 5 minutes
- [ ] Push notification arrives on phone when feeding completes
- [ ] AI Insights tab populates after a few feeding cycles

---

## GPIO Reference

| GPIO | Sensor / Actuator | Type |
|---|---|---|
| 4 | HX711 Data (load cell) | Digital |
| 5 | HX711 Clock (load cell) | Digital |
| 12 | Ultrasonic TRIG | Digital out |
| 14 | Ultrasonic ECHO | Digital in |
| 13 | Servo (food dispenser) | PWM |
| 2 | Hopper LED | Digital out |
| 35 | Hopper LDR | ADC1 (analog in) |
| 34 | Water bowl level sensor | ADC1 (analog in) |
| 33 | Reservoir level sensor | ADC1 (analog in) |
| 26 | Pump relay | Digital out |

> All three analog sensors (GPIO 33, 34, 35) use **ADC1** — these work correctly
> while WiFi is active. Avoid ADC2 pins (0, 2, 4, 12–15, 25–27) for analog reads
> when WiFi is on.

---

## Troubleshooting

| Problem | Likely cause | Fix |
|---|---|---|
| App crashes on launch | `firebase_options.dart` missing | Run `flutterfire configure` (Step 3a) |
| ESP32 stuck on "Connecting to WiFi" | Wrong credentials | Check `WIFI_SSID` / `WIFI_PASSWORD` in `config.h` |
| ESP32 connects but no data in app | Wrong RTDB URL | Check `FIREBASE_DATABASE_URL` in `config.h` and `rtdbUrl` in `app_config.dart` |
| Firebase auth failed on ESP32 | Service account not created | Complete Step 2 |
| Bowl weight reads wrong grams | Calibration factor off | Redo Step 6a |
| Water bowl always 0% or 100% | ADC range not calibrated | Redo Step 6b |
| Reservoir always 0% or 100% | ADC range not calibrated | Redo Step 6c |
| Pump runs with empty reservoir | Reservoir ADC not calibrated | Redo Step 6c |
| Hopper always shows EMPTY | LDR threshold wrong | Redo Step 6d |
| Pet never detected | Distance threshold too low | Increase `PET_DETECTION_DISTANCE_CM` |
| Build fails: "assets not found" | Asset folders missing | Complete Step 3e |
| Build fails: "gradlew not found" | Gradle wrapper missing | Complete Step 3f |
| Cloud Functions deploy fails | Not on Blaze billing plan | Upgrade Firebase project to Blaze |
| Schedules not picked up by ESP32 | `syncSchedulesToRTDB` not deployed | Confirm Step 5 succeeded (4 functions) |
