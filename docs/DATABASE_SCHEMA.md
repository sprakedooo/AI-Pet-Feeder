# Database Schema

## Firebase Architecture

The system uses two Firebase services:
- **Realtime Database (RTDB)**: live sensor data, commands, device state
- **Firestore**: persistent logs, schedules, analytics, user settings

---

## 1. Firebase Realtime Database

Low-latency real-time data shared between ESP32 and mobile app.

```json
{
  "devices": {
    "{deviceId}": {
      "status": {
        "online": true,
        "lastSeen": 1716134400,
        "ipAddress": "192.168.1.100",
        "firmwareVersion": "1.0.0",
        "wifiRSSI": -65
      },
      "sensors": {
        "bowlWeight": 45.2,
        "bowlWeightPercent": 56,
        "waterLevel": 80,
        "hopperStatus": "ok",
        "reservoirStatus": "ok",
        "petDetected": false,
        "petDistance": 120.5,
        "lastUpdated": 1716134400
      },
      "state": {
        "currentState": "IDLE",
        "isDispensing": false,
        "isPumping": false,
        "lastError": "",
        "lastErrorTime": 0,
        "errorCount": 0
      },
      "commands": {
        "manualFeed": false,
        "manualWater": false,
        "emergencyStop": false,
        "tare": false,
        "reboot": false
      },
      "settings_cache": {
        "targetPortionGrams": 80,
        "bowlEmptyThreshold": 20,
        "waterLowThreshold": 30,
        "detectionDistance": 30,
        "timezone": "Asia/Manila",
        "autoMode": true
      }
    }
  }
}
```

---

## 2. Firestore Collections

### users

```
/users/{userId}
  id: string                        # Firebase Auth UID
  email: string
  displayName: string
  photoUrl: string | null
  deviceIds: string[]               # linked device IDs
  fcmToken: string                  # push notification token
  createdAt: timestamp
  lastLoginAt: timestamp
```

### devices

```
/devices/{deviceId}
  id: string
  ownerId: string                   # userId
  name: string                      # "Max's Feeder"
  petName: string                   # "Max"
  petType: string                   # "dog" | "cat" | "other"
  petWeight: number                 # kg (for portion suggestions)
  location: string                  # "Kitchen"
  firmwareVersion: string
  createdAt: timestamp
  lastActiveAt: timestamp
```

### schedules

```
/schedules/{scheduleId}
  id: string
  deviceId: string
  label: string                     # "Morning Feeding"
  hour: number                      # 0–23
  minute: number                    # 0–59
  enabled: boolean
  portionGrams: number              # target grams to dispense
  days: string[]                    # ["mon","tue","wed","thu","fri","sat","sun"]
  createdAt: timestamp
  updatedAt: timestamp
```

### feedingLogs

```
/feedingLogs/{logId}
  id: string
  deviceId: string
  scheduleId: string | null         # null if manual
  triggerType: string               # "scheduled" | "manual" | "retry"
  status: string                    # "success" | "failed" | "skipped" | "partial"
  targetGrams: number
  dispensedGrams: number
  bowlWeightBefore: number
  bowlWeightAfter: number
  retryCount: number
  jamDetected: boolean
  hopperStatus: string
  petDetected: boolean
  durationMs: number
  failReason: string | null
  timestamp: timestamp
```

### waterLogs

```
/waterLogs/{logId}
  id: string
  deviceId: string
  triggerType: string               # "automatic" | "manual"
  status: string                    # "success" | "failed" | "skipped"
  waterLevelBefore: number          # 0–100
  waterLevelAfter: number
  pumpDurationMs: number
  reservoirStatus: string
  failReason: string | null
  timestamp: timestamp
```

### sensorSnapshots

```
/sensorSnapshots/{snapshotId}
  id: string
  deviceId: string
  bowlWeight: number
  bowlWeightPercent: number
  waterLevel: number
  hopperStatus: string
  reservoirStatus: string
  petDetected: boolean
  petDistance: number
  timestamp: timestamp
  hourOfDay: number                 # 0–23 (for aggregation queries)
  dayOfWeek: number                 # 0–6
```

### notifications

```
/notifications/{notificationId}
  id: string
  deviceId: string
  userId: string
  type: string                      # see Notification Types below
  title: string
  body: string
  severity: string                  # "info" | "warning" | "error" | "critical"
  isRead: boolean
  data: map                         # extra context data
  timestamp: timestamp
```

#### Notification Types
```
"food_low"              # Hopper food is running low
"food_empty"            # Hopper is empty
"water_low"             # Bowl water is low
"reservoir_empty"       # Water reservoir is empty
"feeding_success"       # Feeding completed successfully
"feeding_failed"        # Feeding failed after retries
"feeding_skipped"       # Bowl still full, feeding skipped
"jam_detected"          # Servo jam detected
"sensor_error"          # Sensor malfunction
"device_offline"        # ESP32 went offline
"device_online"         # ESP32 came back online
"ai_insight"            # AI recommendation
"overfeeding_risk"      # Anomaly detected
```

### aiInsights

```
/aiInsights/{insightId}
  id: string
  deviceId: string
  type: string                      # "appetite_warning" | "refill_prediction" | "behavior_change" | "dehydration_risk" | "schedule_suggestion"
  title: string
  message: string
  severity: number                  # 1 (info) – 5 (critical)
  isRead: boolean
  isActionable: boolean
  actionLabel: string | null        # e.g., "Adjust Schedule"
  actionRoute: string | null        # e.g., "/schedule"
  dataPoints: map                   # supporting data for the insight
  generatedAt: timestamp
  expiresAt: timestamp
```

### settings

```
/settings/{deviceId}
  deviceId: string
  
  feeding:
    targetPortionGrams: number      # default: 80
    bowlEmptyThreshold: number      # grams below = empty (default: 20)
    maxRetries: number              # default: 3
    servoOpenAngle: number          # default: 90
    servoCloseAngle: number         # default: 0
    dispenseDelay: number           # ms between dispense cycles (default: 2000)
    
  water:
    waterLowThreshold: number       # 0–100, default: 30
    pumpTimeout: number             # ms max pump run time (default: 15000)
    
  pet:
    detectionDistance: number       # cm (default: 30)
    petName: string
    petType: string
    petWeightKg: number
    
  system:
    timezone: string               # "Asia/Manila"
    autoMode: boolean              # true = automatic, false = manual only
    notificationsEnabled: boolean
    sensorInterval: number         # ms between sensor reads (default: 10000)
    
  updatedAt: timestamp
```

---

## 3. Firestore Security Rules

See [backend/firestore.rules](../backend/firestore.rules) for full rules.

Key rules:
- Users can only read/write their own data
- Device data requires ownerId match
- Schedules require deviceId ownership verification
- Sensor snapshots are write-only from ESP32 (service account)

---

## 4. Indexes Required

```json
[
  { "collection": "feedingLogs", "fields": ["deviceId", "timestamp"] },
  { "collection": "feedingLogs", "fields": ["deviceId", "status", "timestamp"] },
  { "collection": "waterLogs",   "fields": ["deviceId", "timestamp"] },
  { "collection": "sensorSnapshots", "fields": ["deviceId", "timestamp"] },
  { "collection": "sensorSnapshots", "fields": ["deviceId", "hourOfDay", "timestamp"] },
  { "collection": "notifications", "fields": ["userId", "isRead", "timestamp"] },
  { "collection": "notifications", "fields": ["deviceId", "timestamp"] },
  { "collection": "aiInsights",  "fields": ["deviceId", "isRead", "generatedAt"] },
  { "collection": "schedules",   "fields": ["deviceId", "enabled"] }
]
```

---

## 5. Data Retention Policy

| Collection | Retention |
|---|---|
| sensorSnapshots | 30 days |
| feedingLogs | 90 days |
| waterLogs | 90 days |
| notifications | 30 days |
| aiInsights | 60 days |
| schedules | permanent |
| settings | permanent |
| users / devices | permanent |

Cleanup can be handled by Firebase Cloud Functions scheduled jobs.
