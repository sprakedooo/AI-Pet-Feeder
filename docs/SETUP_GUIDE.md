# AI Pet Feeder — Complete Setup Guide

Follow these steps **in order**. Each section tells you what you need and exactly what to do.

---

## Getting the Code on a New Machine

```bash
git clone https://github.com/sprakedooo/AI-Pet-Feeder.git
cd AI-Pet-Feeder
```

Everything — including credentials and config files — is in the repo. No extra secrets to copy manually.

---

## Required Software

Install these tools **before** doing anything else:

| Tool | Version | Purpose | Download |
|---|---|---|---|
| **Flutter SDK** | 3.x | Build mobile app | https://flutter.dev/docs/get-started/install |
| **Android Studio** | Latest | Android SDK + build tools | https://developer.android.com/studio |
| **Java JDK** | 17+ | Required by Android build | bundled with Android Studio or https://adoptium.net |
| **VS Code** | Latest | Firmware editor | https://code.visualstudio.com |
| **PlatformIO** extension | Latest | Flash ESP32 firmware | Install from VS Code Extensions → search "PlatformIO IDE" |
| **Python** | 3.8+ | Icon generator script | https://www.python.org/downloads |
| **Node.js** | 18+ | Firebase CLI | https://nodejs.org |
| **Git** | Latest | Clone / version control | https://git-scm.com |

### After installing, run these one-time setup commands:

```bash
# Install Firebase CLI
npm install -g firebase-tools

# Install flutterfire CLI (generates Firebase config for Flutter)
dart pub global activate flutterfire_cli

# Install Python Pillow (used by generate_icon.py)
pip install Pillow

# Verify Flutter is set up correctly
flutter doctor
```

`flutter doctor` will tell you if anything is missing (Android SDK, emulator, etc). Fix all red ✗ items before continuing.

---

## Step 1 — Firebase Project (skip if already done)

> ✅ If you cloned from this repo, Firebase is already configured — skip to Step 3.

1. Go to **https://console.firebase.google.com**
2. Click **Add project** → name it → Continue
3. Disable Google Analytics (optional) → **Create project**

### Enable these services:

**Authentication:**
- Build → Authentication → Get started → Sign-in method → Enable **Email/Password**

**Firestore Database:**
- Build → Firestore Database → Create database → Production mode → choose your region

**Realtime Database:**
- Build → Realtime Database → Create database → Locked mode
- Copy the **Database URL**: `https://your-project-id-default-rtdb.firebaseio.com`

**Cloud Functions:**
- Build → Functions → Get started → upgrade to **Blaze (pay-as-you-go)** plan
  *(Free tier is sufficient for personal use)*

---

## Step 2 — ESP32 Service Account (skip if already done)

> ✅ Already configured in config.h if cloned from this repo.

1. Firebase Console → Authentication → Users tab → **Add user**
2. Email: `device@petfeeder.local` | Password: choose a strong one
3. Copy the password into `firmware/src/config.h`:
   ```cpp
   #define FIREBASE_AUTH_PASSWORD   "your-password-here"
   ```

---

## Step 3 — Mobile App Setup

### 3a. Install Flutter packages

```bash
cd AI-Pet-Feeder/mobile
flutter pub get
```

### 3b. Verify firebase_options.dart exists

```bash
ls lib/firebase_options.dart    # should exist (committed to repo)
```

If it's missing, regenerate it:
```bash
firebase login
flutterfire configure
# Select your Firebase project → Android → accept defaults
```

### 3c. Verify google-services.json exists

```bash
ls android/app/google-services.json    # should exist (committed to repo)
```

If it's missing:
1. Firebase Console → Project Settings → Your apps → Android app
2. Download **google-services.json** → place at `android/app/google-services.json`

### 3d. Build and install

Run directly on a connected phone:
```bash
flutter run
```

Or build a release APK to install manually:
```bash
flutter build apk --release --no-tree-shake-icons
# APK saved to: build/app/outputs/flutter-apk/app-release.apk
```

Transfer the APK to your Android phone and tap to install.
*(You may need to enable "Install from unknown sources" in Android settings.)*

---

## Step 4 — Firmware Setup

### 4a. Open in VS Code + PlatformIO

1. Open VS Code
2. File → Open Folder → select `AI-Pet-Feeder/firmware`
3. PlatformIO will automatically install all ESP32 libraries (first time takes ~5 minutes)

### 4b. Check config.h

Open `firmware/src/config.h` — verify these match your setup:

```cpp
// WiFi
#define WIFI_SSID           "YourWiFiName"
#define WIFI_PASSWORD       "YourWiFiPassword"

// Firebase (already filled in from repo)
#define FIREBASE_API_KEY         "AIzaSy..."
#define FIREBASE_DATABASE_URL    "https://...firebaseio.com"
#define FIREBASE_AUTH_EMAIL      "device@petfeeder.local"
#define FIREBASE_AUTH_PASSWORD   "your-password"
```

### 4c. Flash to ESP32

1. Connect ESP32 via USB
2. Click the **→ Upload** button in PlatformIO toolbar (or press `Ctrl+Alt+U`)
3. Wait for upload — should say `[SUCCESS]` at the end
4. Open Serial Monitor at **115200 baud** — you should see:

```
[WiFi] Connecting to YourWiFiName...
[WiFi] Connected. IP: 192.168.x.x
[Firebase] Connected.
[StateMachine] State: IDLE
```

---

## Step 5 — Deploy Firebase Backend

```bash
cd AI-Pet-Feeder/backend
firebase login
firebase use --add    # select your project when prompted

cd functions
npm install
cd ..

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

Do this **after wiring everything** and **after flashing firmware**.
Open PlatformIO Serial Monitor at **115200 baud** to see live readings.

### 6a. Load Cell — Food Bowl Weight (GPIO 4 & 5)

1. Place **empty bowl** on load cell → power on (firmware auto-tares on boot)
2. Place a **known weight** (e.g. 100g) on the bowl
3. Watch Serial Monitor for the raw reading
4. Calculate: `CALIBRATION_FACTOR = raw_reading / known_weight_grams`
5. Update `config.h`: `#define LOAD_CELL_CALIBRATION_FACTOR  420.0f`
6. Re-flash and verify reading matches known weight

### 6b. Water Bowl Sensor (GPIO 34)

1. **Empty bowl** → note ADC value → `WATER_EMPTY_ADC`
2. **Full bowl** → note ADC value → `WATER_FULL_ADC`
3. Update `config.h`:
   ```cpp
   #define WATER_EMPTY_ADC    100
   #define WATER_FULL_ADC    1800
   ```

### 6c. Reservoir Sensor (GPIO 33)

Same process as 6b for the refill tank:
```cpp
#define RESERVOIR_EMPTY_ADC    100
#define RESERVOIR_FULL_ADC    1800
```

### 6d. Hopper Food Sensor / LDR (GPIO 35)

1. **Full hopper** (beam blocked) → note ADC reading
2. **Empty hopper** (beam free) → note ADC reading
3. Threshold = `(full_reading + empty_reading) / 2`
4. Update: `#define HOPPER_FOOD_PRESENT_ADC  900`

### 6e. Pet Detection — Ultrasonic (GPIO 12 & 14)

1. Mount sensor **above bowl pointing down**
2. With **no pet present**, note distance reading (e.g. 42 cm)
3. Set threshold to ~70% of mounting height: `PET_DETECTION_DISTANCE_CM = height × 0.70`
4. Update: `#define PET_DETECTION_DISTANCE_CM  30`

---

## GPIO Reference

| GPIO | Sensor / Actuator | Notes |
|---|---|---|
| 4 | HX711 Data (load cell) | Digital |
| 5 | HX711 Clock (load cell) | Digital |
| 12 | Ultrasonic TRIG | Digital out |
| 13 | Servo (food dispenser) | PWM |
| 14 | Ultrasonic ECHO | Digital in |
| 25 | **Physical feed button** | Digital in, INPUT_PULLUP — wire to GND |
| 26 | Pump relay | Digital out (active LOW) |
| 27 | Hopper LED | Digital out |
| 33 | Reservoir level sensor | ADC1 analog in |
| 34 | Water bowl level sensor | ADC1 analog in |
| 35 | Hopper LDR | ADC1 analog in |

**Physical button wiring:**
```
ESP32 GPIO 25 ──── [momentary push button] ──── GND
```
No resistor needed — firmware uses internal pull-up. Press to manually dispense food.

> All analog sensors use **ADC1** (GPIO 32–39) — safe while WiFi is active.
> Never use ADC2 pins for analog reads when WiFi is on.

---

## First Run Checklist

- [ ] App opens and shows login/register screen
- [ ] Login works and dashboard loads
- [ ] Dashboard shows live sensor readings (not all zeros)
- [ ] Sensor values update every ~10 seconds
- [ ] "Feed Now" button triggers the servo and bowl weight increases
- [ ] "Water Now" button starts pump and stops at full level
- [ ] Pump does NOT start when reservoir is below 10%
- [ ] Physical button on GPIO 25 triggers feeding
- [ ] Feeding schedule created in app is picked up by ESP32 within 5 minutes
- [ ] AI Insights tab populates after a few feedings
- [ ] Push notification arrives on phone when feeding completes

---

## Troubleshooting

| Problem | Likely cause | Fix |
|---|---|---|
| `flutter doctor` shows errors | Missing SDK tools | Follow the fix suggestions it prints |
| App crashes on launch | `firebase_options.dart` wrong | Re-run `flutterfire configure` |
| ESP32 stuck on "Connecting to WiFi" | Wrong credentials | Check `WIFI_SSID` / `WIFI_PASSWORD` in `config.h` |
| ESP32 connects but no data in app | Wrong RTDB URL | Check `FIREBASE_DATABASE_URL` in `config.h` |
| Schedule / Insights shows loading forever | Firestore index missing | Make sure Step 5 (deploy) succeeded |
| Bowl weight reads wrong | Calibration factor off | Redo Step 6a |
| Water always 0% or 100% | ADC not calibrated | Redo Step 6b |
| Physical button not responding | Wrong GPIO or not grounded | Check wiring — GPIO 25 to GND via button |
| Build fails: disk full | Gradle cache too large | Delete `~/.gradle/caches/build-cache-1` |
| Cloud Functions deploy fails | Not on Blaze plan | Upgrade Firebase project to Blaze |
