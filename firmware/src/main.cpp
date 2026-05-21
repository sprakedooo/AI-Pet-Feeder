#include <Arduino.h>
#include "config.h"

// Network
#include "network/WiFiManager.h"
#include "network/FirebaseManager.h"

// Sensors
#include "managers/SensorManager.h"

// Actuators
#include "actuators/ServoController.h"
#include "actuators/PumpController.h"

// Managers
#include "managers/FeedingManager.h"
#include "managers/WaterManager.h"

// AI
#include "ai/AIEngine.h"

// State machine
#include "state/StateMachine.h"

// ── Module instances ──────────────────────────────────────────────
WiFiManager    wifiMgr;
FirebaseManager firebase;

SensorManager  sensors;
ServoController servo;
PumpController  pump;

FeedingManager  feeding(servo, sensors);
WaterManager    water(pump, sensors);

AIEngine        ai;

StateMachine    stateMachine(sensors, feeding, water, firebase, ai);

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n====================================");
    Serial.println("  AI Smart Pet Feeder v" FIRMWARE_VERSION);
    Serial.println("====================================\n");

    // 1. Initialize actuators first (ensure safe state)
    servo.begin();
    pump.begin();

    // 2. Initialize sensors
    sensors.begin();

    // 3. Connect WiFi
    wifiMgr.begin();

    // 4. Initialize Firebase (requires WiFi)
    if (wifiMgr.isConnected()) {
        firebase.begin();
    }

    // 5. Initialize AI engine
    ai.begin();

    // 6. Start state machine
    stateMachine.begin();

    Serial.println("\n[Main] System ready.\n");
}

// ── Main Loop ─────────────────────────────────────────────────────
void loop() {
    // 1. Maintain WiFi connection
    wifiMgr.update();

    // 2. Maintain Firebase connection
    firebase.update();

    // 3. Update state machine (drives all logic)
    stateMachine.update();

    // 4. Small yield to prevent watchdog resets
    delay(10);
}
