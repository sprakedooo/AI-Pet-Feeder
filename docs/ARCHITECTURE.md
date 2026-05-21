# System Architecture

## 1. High-Level Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                        SYSTEM LAYERS                                  │
│                                                                        │
│  ┌─────────────┐    ┌─────────────────┐    ┌──────────────────────┐  │
│  │  HARDWARE   │    │   ESP32 FW      │    │   CLOUD (Firebase)   │  │
│  │             │    │                 │    │                      │  │
│  │  Load Cell  │───▶│  SensorManager  │───▶│  Realtime Database   │  │
│  │  Water Lvl  │    │  FeedingManager │    │  Firestore           │  │
│  │  Ultrasonic │    │  WaterManager   │◀───│  Auth                │  │
│  │  LDR/LED    │    │  AIEngine       │    │  FCM (Notifications) │  │
│  │  Float Sw.  │    │  StateMachine   │    │                      │  │
│  │  Servo      │◀───│  WiFiManager    │    └──────────┬───────────┘  │
│  │  Pump+Relay │    │  FirebaseManager│               │              │
│  └─────────────┘    └─────────────────┘    ┌──────────▼───────────┐  │
│                                             │   MOBILE APP         │  │
│                                             │   (Flutter)          │  │
│                                             │                      │  │
│                                             │  Dashboard           │  │
│                                             │  Scheduler           │  │
│                                             │  AI Insights         │  │
│                                             │  History             │  │
│                                             │  Notifications       │  │
│                                             │  Settings            │  │
│                                             └──────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 2. ESP32 Firmware Architecture

### 2.1 State Machine

```
                    ┌──────────┐
              ┌────▶│   IDLE   │◀────────────────────────┐
              │     └────┬─────┘                         │
              │          │ every 30s                      │
              │          ▼                                │
              │  ┌────────────────┐                       │
              │  │CHECK_SCHEDULE  │                       │
              │  └────────┬───────┘                       │
              │           │ schedule match                 │
              │           ▼                               │
              │  ┌────────────────┐    bowl already full  │
              │  │ PRE_FEED_CHECK │──────────────────────▶│
              │  └────────┬───────┘                       │
              │           │ all clear                     │
              │           ▼                               │
              │  ┌────────────────┐     max retries       │
        error │  │  DISPENSING    │──────────────────────▶┤
              │  └────────┬───────┘                       │
              │           │ dispense done                  │
              │           ▼                               │
              │  ┌────────────────┐      not enough food  │
              │  │ FEED_VERIFY    │──────────────────────▶┤ DISPENSING
              │  └────────┬───────┘                       │  (retry)
              │           │ verified                      │
              │           ▼                               │
              │  ┌────────────────┐                       │
              │  │  WATER_CHECK   │                       │
              │  └────────┬───────┘                       │
              │           │ low water                     │
              │           ▼                               │
              │  ┌────────────────┐    reservoir empty    │
              │  │ WATER_PUMPING  │──────────────────────▶│
              │  └────────┬───────┘                       │
              │           │ bowl filled                   │
              └───────────┘                               │
                                                          │
              ┌───────────────────────────────────────────┘
              ▼
        ┌───────────┐
        │   ERROR   │──▶ notify + log + recover
        └───────────┘
```

### 2.2 Firmware Module Map

```
src/
├── main.cpp                    # Setup + main loop
├── config.h                    # All pins, thresholds, timing constants
│
├── sensors/
│   ├── LoadCellManager         # HX711 driver + tare + averaging
│   ├── WaterLevelSensor        # Analog ADC read + percentage
│   ├── UltrasonicSensor        # HC-SR04 distance measurement
│   ├── HopperSensor            # LDR + LED light barrier
│   └── FloatSensor             # Digital GPIO reed/float switch
│
├── actuators/
│   ├── ServoController         # PWM servo + anti-jam logic
│   └── PumpController          # Relay ON/OFF + timeout safety
│
├── managers/
│   ├── SensorManager           # Polls all sensors, exposes data
│   ├── FeedingManager          # Feeding logic, retry, verification
│   └── WaterManager            # Water refill logic, dry-run protection
│
├── network/
│   ├── WiFiManager             # Connect + auto-reconnect
│   └── FirebaseManager         # RTDB sync, command listener, FCM
│
├── ai/
│   └── AIEngine                # Rule-based analysis + recommendations
│
└── state/
    └── StateMachine            # Central state coordinator
```

---

## 3. Mobile App Architecture

### 3.1 Screen Flow

```
Splash ──▶ Auth (Login/Register)
                │
                ▼
           Dashboard (Home)
           ├── Schedule Manager
           ├── Feeding History
           ├── AI Insights
           ├── Notifications
           └── Settings
```

### 3.2 State Management (Provider)

```
AuthProvider          ──▶  login, logout, user state
DeviceProvider        ──▶  device online/offline, status
SensorProvider        ──▶  real-time sensor readings (RTDB stream)
FeedingProvider       ──▶  schedules CRUD, feeding logs
NotificationProvider  ──▶  unread count, notification list
AIProvider            ──▶  insights, recommendations
```

### 3.3 Service Layer

```
AuthService           ──▶  Firebase Auth
DatabaseService       ──▶  Firestore CRUD + RTDB streams
NotificationService   ──▶  FCM token registration, local notifs
AIService             ──▶  rule evaluation, insight generation
```

---

## 4. Firebase Database Architecture

```
Firebase Realtime Database (RTDB) — low-latency live data:

/devices/{deviceId}/
    status/
        online: bool
        lastSeen: timestamp
        ipAddress: string
        firmwareVersion: string
    sensors/
        bowlWeight: float         # grams
        bowlWeightPercent: int    # 0-100
        waterLevel: int           # 0-100
        hopperStatus: string      # "ok" | "low" | "empty"
        reservoirStatus: string   # "ok" | "empty"
        petDetected: bool
        lastUpdated: timestamp
    state/
        currentState: string
        isDispensing: bool
        isPumping: bool
        lastError: string
    commands/
        manualFeed: bool          # app writes true → ESP reads + clears
        manualWater: bool
        emergencyStop: bool
        
Firestore — persistent data:

/users/{userId}/devices/{deviceId}
/schedules/{scheduleId}
/feedingLogs/{logId}
/waterLogs/{logId}
/notifications/{notificationId}
/aiInsights/{insightId}
/settings/{deviceId}
```

---

## 5. AI Engine Architecture

### Rule Engine (Lightweight, No ML)

```
Data Inputs:
  - feeding_logs[]
  - water_logs[]
  - sensor_snapshots[]

Rule Processing:
  ├── Appetite Monitor
  │     IF bowl_full_duration > 4h THEN → appetite_warning
  │
  ├── Consumption Tracker
  │     IF daily_intake < 7_day_avg * 0.7 THEN → low_appetite_alert
  │
  ├── Hopper Predictor
  │     refill_rate = avg(hopper_empty_events per week)
  │     THEN → predict_next_refill
  │
  ├── Water Consumption Monitor
  │     IF daily_water < threshold THEN → dehydration_risk_alert
  │
  ├── Schedule Adherence Monitor
  │     IF skipped_meals > 2 consecutive THEN → alert
  │
  └── Feeding Frequency Analyzer
        IF feeding_count > normal_range THEN → overfeeding_alert

Output: AIInsight[]
  - type: "warning" | "info" | "prediction" | "alert"
  - message: string
  - severity: 1-5
  - timestamp
```

---

## 6. Communication Protocol

### ESP32 → Firebase (Push)
- Sensor data: every 10 seconds
- State changes: immediately on change
- Feeding logs: after each feeding event
- AI data: every 30 minutes
- Heartbeat: every 60 seconds

### Firebase → ESP32 (Pull/Listen)
- Commands (manual feed, manual water): listen on RTDB
- Schedule updates: listen on Firestore
- Settings changes: listen on RTDB

### Firebase → Mobile (Stream)
- Sensor data: RTDB stream (real-time)
- Device status: RTDB stream
- Notifications: FCM push

---

## 7. Power Architecture

```
External 5V/2A Supply ──┬──▶ ESP32 VIN
                        ├──▶ Servo Motor (direct)
                        └──▶ Relay VCC (control side)

External 6V/1A Supply ──────▶ Relay NO/COM ──▶ Water Pump

Logic Level:
  ESP32 GPIO ──▶ Relay IN pin (3.3V signal, active LOW)
  ESP32 3.3V ──▶ Sensors (Load Cell, Ultrasonic, LDR, Float)
```
