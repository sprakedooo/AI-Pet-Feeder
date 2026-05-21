# AI-Powered Smart Pet Feeder and Water Monitoring System

A complete IoT-based intelligent pet feeder combining ESP32 firmware, a Flutter mobile app, and Firebase cloud backend.

---

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    SMART PET FEEDER SYSTEM                      │
├─────────────┬───────────────────────┬───────────────────────────┤
│  ESP32      │     Firebase Cloud    │     Flutter Mobile App    │
│  Firmware   │     Backend           │                           │
│             │                       │                           │
│  Sensors    │  Realtime DB (RTDB)   │  Dashboard                │
│  Actuators  │  Firestore            │  Schedule Manager         │
│  State M/C  │  FCM Notifications    │  AI Insights              │
│  AI Engine  │  Firebase Auth        │  History & Logs           │
│  WiFi Sync  │  Cloud Functions      │  Settings                 │
└─────────────┴───────────────────────┴───────────────────────────┘
```

---

## Project Structure

```
AI PET FEEDER/
├── firmware/                    # ESP32 Arduino firmware (PlatformIO)
│   ├── platformio.ini
│   └── src/
│       ├── main.cpp
│       ├── config.h
│       ├── sensors/
│       ├── actuators/
│       ├── managers/
│       ├── network/
│       ├── ai/
│       └── state/
├── mobile/                      # Flutter mobile application
│   ├── pubspec.yaml
│   └── lib/
│       ├── main.dart
│       ├── config/
│       ├── models/
│       ├── services/
│       ├── providers/
│       ├── screens/
│       └── widgets/
├── backend/                     # Firebase configuration
│   ├── firestore.rules
│   ├── firestore.indexes.json
│   └── functions/
└── docs/
    ├── ARCHITECTURE.md
    ├── WIRING_GUIDE.md
    ├── GPIO_MAP.md
    ├── DATABASE_SCHEMA.md
    └── SETUP_GUIDE.md
```

---

## Hardware Components

| Component | Purpose |
|---|---|
| ESP32 Dev Board | Main microcontroller |
| HX711 + Load Cell | Bowl food weight measurement |
| Water Level Sensor | Bowl water level |
| HC-SR04 Ultrasonic | Pet presence detection |
| LED + LDR | Hopper food level detection |
| DIY Float Sensor | Water reservoir monitoring |
| Servo Motor | Food dispensing flap |
| 6V Submersible Pump | Water dispensing |
| Relay Module | Pump control |
| External Power Supply | Powers servo, pump, relay |

---

## Quick Start

1. Read [docs/SETUP_GUIDE.md](docs/SETUP_GUIDE.md)
2. Wire hardware per [docs/WIRING_GUIDE.md](docs/WIRING_GUIDE.md)
3. Flash firmware (see [firmware/](firmware/))
4. Build & run mobile app (see [mobile/](mobile/))
5. Configure Firebase (see [backend/](backend/))

---

## Features

- Automatic food dispensing with anti-overfeeding logic
- Scheduled feeding (up to 6 schedules/day)
- Smart water refill automation
- Pet presence detection
- Hopper low-food monitoring
- Water reservoir monitoring
- Real-time mobile dashboard
- Push notifications
- Feeding history and analytics
- Rule-based AI recommendations
- Manual control override
- Error detection and recovery

---

## Tech Stack

- **Firmware**: C++ / Arduino framework / PlatformIO
- **Mobile**: Flutter (Dart)
- **Backend**: Firebase (Auth + RTDB + Firestore + FCM)
- **Cloud AI**: Rule-based analytics engine

---

## License

MIT — for educational and personal use.
