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

// ── Physical feed button state ────────────────────────────────────
static int           _btnLastState    = HIGH; // pull-up → idle = HIGH
static int           _btnStableState  = HIGH;
static unsigned long _btnDebounceAt   = 0;
static unsigned long _btnLastTrigger  = 0;

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

    // 7. Configure physical feed button (active-LOW, internal pull-up)
    pinMode(PIN_MANUAL_FEED_BTN, INPUT_PULLUP);

    // 8. Final safety close — ensures servo is at 0° after all init,
    //    regardless of any boot-time GPIO glitches (e.g. serial monitor DTR reset).
    servo.close();

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

    // 4. Handle WiFi reset command (clears NVS credentials → reboots into AP mode)
    if (stateMachine.shouldResetWifi()) {
        Serial.println("[Main] Resetting WiFi credentials and rebooting into AP setup mode...");
        wifiMgr.clearCredentials();
        delay(1000);
        ESP.restart();
    }

    // 5. Physical feed button — debounced, active-LOW
    {
        int reading = digitalRead(PIN_MANUAL_FEED_BTN);

        // Reset debounce timer whenever the raw reading changes
        if (reading != _btnLastState) {
            _btnDebounceAt = millis();
            _btnLastState  = reading;
        }

        // Only act after the signal has been stable for BTN_DEBOUNCE_MS
        if ((millis() - _btnDebounceAt) >= BTN_DEBOUNCE_MS) {
            // Detect falling edge: button just pressed (HIGH → LOW)
            if (reading == LOW && _btnStableState == HIGH) {
                unsigned long now = millis();
                if ((now - _btnLastTrigger) >= BTN_COOLDOWN_MS) {
                    _btnLastTrigger = now;
                    Serial.println("[BTN] Physical feed button pressed — requesting manual feed.");
                    stateMachine.requestManualFeed();
                } else {
                    Serial.println("[BTN] Button press ignored (cooldown active).");
                }
            }
            _btnStableState = reading;
        }
    }

    // 6. Small yield to prevent watchdog resets
    delay(10);
}
