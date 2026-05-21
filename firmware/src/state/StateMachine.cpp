#include "StateMachine.h"

StateMachine::StateMachine(SensorManager&  sensors,
                            FeedingManager& feeding,
                            WaterManager&   water,
                            FirebaseManager& firebase,
                            AIEngine&        ai)
    : _sensors(sensors), _feeding(feeding), _water(water),
      _firebase(firebase), _ai(ai),
      _state(SystemState::IDLE), _prevState(SystemState::IDLE),
      _scheduleCount(0),
      _lastScheduleCheck(0), _lastSensorSync(0),
      _lastHeartbeat(0), _lastAIRun(0), _lastScheduleLoad(0),
      _manualFeedRequested(false), _manualWaterRequested(false),
      _emergencyStopRequested(false) {
    memset(_schedules, 0, sizeof(_schedules));
}

void StateMachine::begin() {
    _transition(SystemState::IDLE);
    Serial.println("[StateMachine] Started.");
    loadSchedules();
}

void StateMachine::update() {
    // Always check for emergency stop first
    if (_emergencyStopRequested) {
        _water.emergencyStop();
        _transition(SystemState::EMERGENCY_STOP);
        _emergencyStopRequested = false;
        _firebase.pushState("EMERGENCY_STOP", false, false);
        _firebase.sendNotification("emergency_stop", "Emergency Stop", "System emergency stop triggered.");
        delay(2000);
        _transition(SystemState::IDLE);
        return;
    }

    // Read sensors and sync to Firebase periodically
    _sensors.update();
    _runPeriodicTasks();

    // Process remote commands from Firebase
    _processCommands();

    // Handle manual requests with priority
    if (_manualFeedRequested && _state == SystemState::IDLE) {
        _manualFeedRequested = false;
        _transition(SystemState::MANUAL_FEED);
    }
    if (_manualWaterRequested && _state == SystemState::IDLE) {
        _manualWaterRequested = false;
        _transition(SystemState::MANUAL_WATER);
    }

    // State machine dispatch
    switch (_state) {
        case SystemState::IDLE:
            _handleIdle();
            break;
        case SystemState::CHECKING_SCHEDULE:
            _handleCheckingSchedule();
            break;
        case SystemState::PRE_FEED_CHECK:
            _handlePreFeedCheck();
            break;
        case SystemState::DISPENSING:
            _handleDispensing();
            break;
        case SystemState::WATER_CHECK:
            _handleWaterCheck();
            break;
        case SystemState::MANUAL_FEED:
            _handleManualFeed();
            break;
        case SystemState::MANUAL_WATER:
            _handleManualWater();
            break;
        case SystemState::ERROR:
            _handleError();
            break;
        default:
            _transition(SystemState::IDLE);
            break;
    }
}

// ── State Handlers ────────────────────────────────────────────────

void StateMachine::_handleIdle() {
    unsigned long now = millis();
    if (now - _lastScheduleCheck >= SCHEDULE_CHECK_INTERVAL_MS) {
        _lastScheduleCheck = now;
        _transition(SystemState::CHECKING_SCHEDULE);
    }
}

void StateMachine::_handleCheckingSchedule() {
    if (!_firebase.isReady()) {
        _transition(SystemState::IDLE);
        return;
    }

    if (_isScheduleMatch()) {
        Serial.println("[StateMachine] Schedule matched — starting feeding.");
        _transition(SystemState::PRE_FEED_CHECK);
    } else {
        // No schedule match — check water instead
        _transition(SystemState::WATER_CHECK);
    }
}

void StateMachine::_handlePreFeedCheck() {
    const SensorData& data = _sensors.getData();

    // Bowl already full — skip feeding
    if (data.bowlWeightGrams >= BOWL_FULL_THRESHOLD_GRAMS) {
        Serial.printf("[StateMachine] Bowl full (%.1fg) — skipping feeding.\n",
                      data.bowlWeightGrams);
        _ai.recordFeeding(0, true, true);
        _firebase.sendNotification("feeding_skipped",
            "Feeding Skipped",
            "Bowl still has food. New feeding skipped to prevent overfeeding.");
        _transition(SystemState::WATER_CHECK);
        return;
    }

    // Hopper empty — cannot feed
    if (!data.hopperStatus != (int)HopperStatus::EMPTY && !_sensors.hopper.hasFeed()) {
        Serial.println("[StateMachine] Hopper empty — cannot feed.");
        _firebase.sendNotification("food_empty",
            "Hopper Empty",
            "The food hopper is empty. Please refill it.");
        _transition(SystemState::ERROR);
        return;
    }

    _transition(SystemState::DISPENSING);
}

void StateMachine::_handleDispensing() {
    float portion = _scheduledPortion();
    _firebase.pushState("DISPENSING", true, false);

    FeedResult result = _feeding.dispense(portion);
    const FeedingRecord& rec = _feeding.getLastRecord();

    _firebase.pushFeedingLog(rec);
    _firebase.pushState("IDLE", false, false);

    switch (result) {
        case FeedResult::SUCCESS:
            Serial.println("[StateMachine] Feeding SUCCESS.");
            _ai.recordFeeding(rec.dispensedGrams, false,
                              rec.bowlAfter < BOWL_EMPTY_THRESHOLD_GRAMS);
            _firebase.sendNotification("feeding_success",
                "Feeding Complete",
                "Food dispensed successfully.");
            break;

        case FeedResult::FAILED_JAM:
        case FeedResult::FAILED_TIMEOUT:
        case FeedResult::FAILED_NO_WEIGHT_CHANGE:
            Serial.printf("[StateMachine] Feeding FAILED: %d\n", (int)result);
            _firebase.sendNotification("feeding_failed",
                "Feeding Failed",
                "Could not dispense food. Please check the feeder.");
            _transition(SystemState::ERROR);
            return;

        default:
            break;
    }

    // After feeding, check water
    _resetScheduleTriggers(_firebase.getCurrentHour(), _firebase.getCurrentMinute());
    _transition(SystemState::WATER_CHECK);
}

void StateMachine::_handleWaterCheck() {
    const SensorData& data = _sensors.getData();

    if (data.waterLevelPercent < WATER_LOW_THRESHOLD) {
        if (!data.reservoirHasWater) {
            Serial.println("[StateMachine] Reservoir empty — cannot refill.");
            _firebase.sendNotification("reservoir_empty",
                "Reservoir Empty",
                "The water reservoir is empty. Please refill.");
            _transition(SystemState::IDLE);
            return;
        }

        Serial.printf("[StateMachine] Water low (%d%%) — pumping.\n",
                      data.waterLevelPercent);
        _firebase.pushState("WATER_PUMPING", false, true);

        WaterResult result = _water.refill();
        const WaterRecord& rec = _water.getLastRecord();
        _firebase.pushWaterLog(rec);
        _firebase.pushState("IDLE", false, false);

        if (result == WaterResult::SUCCESS) {
            _firebase.sendNotification("water_refilled",
                "Water Refilled",
                "Bowl water has been automatically refilled.");
        } else if (result == WaterResult::FAILED_TIMEOUT) {
            _firebase.sendNotification("water_failed",
                "Water Refill Failed",
                "Pump timed out. Please check the system.");
        }
    }

    // Also send low-food hopper warning if needed
    if (data.hopperStatus == HopperStatus::LOW) {
        _firebase.sendNotification("food_low",
            "Food Level Low",
            "The food hopper is running low. Please refill soon.");
    }

    _transition(SystemState::IDLE);
}

void StateMachine::_handleManualFeed() {
    Serial.println("[StateMachine] Manual feed triggered.");
    _firebase.pushState("MANUAL_FEED", true, false);

    DeviceSettings settings = _firebase.getSettings();
    FeedResult result = _feeding.dispense(settings.targetPortionGrams);
    const FeedingRecord& rec = _feeding.getLastRecord();

    _firebase.pushFeedingLog(rec);
    _firebase.pushState("IDLE", false, false);

    if (result == FeedResult::SUCCESS) {
        _firebase.sendNotification("feeding_success",
            "Manual Feed Complete", "Food dispensed manually.");
    } else if (result == FeedResult::SKIPPED_BOWL_FULL) {
        _firebase.sendNotification("feeding_skipped",
            "Manual Feed Skipped", "Bowl is already full.");
    } else {
        _firebase.sendNotification("feeding_failed",
            "Manual Feed Failed", "Could not dispense food.");
    }

    _transition(SystemState::IDLE);
}

void StateMachine::_handleManualWater() {
    Serial.println("[StateMachine] Manual water triggered.");
    _firebase.pushState("MANUAL_WATER", false, true);

    WaterResult result = _water.refill();
    const WaterRecord& rec = _water.getLastRecord();

    _firebase.pushWaterLog(rec);
    _firebase.pushState("IDLE", false, false);

    if (result == WaterResult::SUCCESS) {
        _firebase.sendNotification("water_refilled",
            "Manual Water Complete", "Bowl water refilled manually.");
    } else {
        _firebase.sendNotification("water_failed",
            "Manual Water Failed", "Could not refill water.");
    }

    _transition(SystemState::IDLE);
}

void StateMachine::_handleError() {
    Serial.println("[StateMachine] In ERROR state — recovering in 30s.");
    delay(30000);
    _transition(SystemState::IDLE);
}

// ── Periodic Tasks ────────────────────────────────────────────────

void StateMachine::_runPeriodicTasks() {
    unsigned long now = millis();

    // Sync sensor data to Firebase
    if (now - _lastSensorSync >= FIREBASE_SYNC_INTERVAL_MS) {
        _lastSensorSync = now;
        _syncToFirebase();
    }

    // Heartbeat
    if (now - _lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        _lastHeartbeat = now;
        _firebase.pushHeartbeat();
    }

    // AI analysis
    if (now - _lastAIRun >= AI_ANALYSIS_INTERVAL_MS) {
        _lastAIRun = now;
        _runAIAnalysis();
    }

    // Reload schedules from Firebase every 5 minutes
    if (now - _lastScheduleLoad >= 300000UL) {
        _lastScheduleLoad = now;
        loadSchedules();
    }

    // Water manager safety watchdog
    _water.update();
}

void StateMachine::_syncToFirebase() {
    if (!_firebase.isReady()) return;
    _firebase.pushSensorData(_sensors.getData());
}

void StateMachine::_runAIAnalysis() {
    const int MAX_INSIGHTS = 5;
    AIInsight insights[MAX_INSIGHTS];
    int count = _ai.analyze(insights, MAX_INSIGHTS);

    for (int i = 0; i < count; i++) {
        _firebase.sendNotification("ai_insight",
            insights[i].title,
            insights[i].message);
    }
}

void StateMachine::_processCommands() {
    if (!_firebase.isReady()) return;
    if (_state != SystemState::IDLE) return;  // Only process commands when idle

    PendingCommands cmds = _firebase.checkCommands();

    if (cmds.emergencyStop) {
        _emergencyStopRequested = true;
    }
    if (cmds.manualFeed) {
        _manualFeedRequested = true;
    }
    if (cmds.manualWater) {
        _manualWaterRequested = true;
    }
    if (cmds.tare) {
        _sensors.tareScale();
        _firebase.sendNotification("info", "Scale Tared", "Load cell has been re-tared.");
    }
    if (cmds.reboot) {
        Serial.println("[StateMachine] Reboot command received.");
        delay(1000);
        ESP.restart();
    }
}

// ── Schedule Helpers ──────────────────────────────────────────────

bool StateMachine::_isScheduleMatch() {
    int hour   = _firebase.getCurrentHour();
    int minute = _firebase.getCurrentMinute();

    for (int i = 0; i < _scheduleCount; i++) {
        if (!_schedules[i].enabled) continue;
        if (_schedules[i].triggered) continue;
        if (_schedules[i].hour   == hour &&
            _schedules[i].minute == minute) {
            _schedules[i].triggered = true;
            return true;
        }
    }
    return false;
}

float StateMachine::_scheduledPortion() {
    int hour   = _firebase.getCurrentHour();
    int minute = _firebase.getCurrentMinute();

    for (int i = 0; i < _scheduleCount; i++) {
        if (_schedules[i].hour == hour && _schedules[i].minute == minute) {
            return _schedules[i].portionGrams > 0
                   ? _schedules[i].portionGrams
                   : DEFAULT_TARGET_GRAMS;
        }
    }
    return DEFAULT_TARGET_GRAMS;
}

void StateMachine::_resetScheduleTriggers(int hour, int minute) {
    // Reset triggers every full hour to allow next day's match
    for (int i = 0; i < _scheduleCount; i++) {
        if (_schedules[i].hour != hour || _schedules[i].minute != minute) {
            _schedules[i].triggered = false;
        }
    }
}

void StateMachine::loadSchedules() {
    // For simplicity, load hardcoded defaults; in production read from Firebase
    // The mobile app writes schedules; this ESP32 reads from RTDB

    _scheduleCount = 0;

    // Try to read from Firebase RTDB
    if (!_firebase.isReady()) {
        Serial.println("[StateMachine] Firebase not ready — using default schedule.");
        // Default: 8:00 AM and 6:00 PM
        _schedules[0] = {true, 8, 0, DEFAULT_TARGET_GRAMS, false};
        _schedules[1] = {true, 18, 0, DEFAULT_TARGET_GRAMS, false};
        _scheduleCount = 2;
        return;
    }

    // In a full implementation, Firebase RTDB would return schedule JSON here.
    // For now, default schedules are used until overridden by the mobile app.
    _schedules[0] = {true, 8, 0, DEFAULT_TARGET_GRAMS, false};
    _schedules[1] = {true, 18, 0, DEFAULT_TARGET_GRAMS, false};
    _scheduleCount = 2;

    Serial.printf("[StateMachine] Loaded %d schedule(s).\n", _scheduleCount);
}

// ── Utility ───────────────────────────────────────────────────────

void StateMachine::_transition(SystemState next) {
    if (_state == next) return;
    Serial.printf("[StateMachine] %s → %s\n", stateString(),
        [next]() -> const char* {
            switch(next) {
                case SystemState::IDLE:               return "IDLE";
                case SystemState::CHECKING_SCHEDULE:  return "CHECKING_SCHEDULE";
                case SystemState::PRE_FEED_CHECK:     return "PRE_FEED_CHECK";
                case SystemState::DISPENSING:         return "DISPENSING";
                case SystemState::WATER_CHECK:        return "WATER_CHECK";
                case SystemState::MANUAL_FEED:        return "MANUAL_FEED";
                case SystemState::MANUAL_WATER:       return "MANUAL_WATER";
                case SystemState::ERROR:              return "ERROR";
                case SystemState::EMERGENCY_STOP:     return "EMERGENCY_STOP";
                default:                              return "UNKNOWN";
            }
        }()
    );
    _prevState = _state;
    _state     = next;
}

SystemState StateMachine::getState() const {
    return _state;
}

const char* StateMachine::stateString() const {
    switch (_state) {
        case SystemState::IDLE:               return "IDLE";
        case SystemState::CHECKING_SCHEDULE:  return "CHECKING_SCHEDULE";
        case SystemState::PRE_FEED_CHECK:     return "PRE_FEED_CHECK";
        case SystemState::DISPENSING:         return "DISPENSING";
        case SystemState::WATER_CHECK:        return "WATER_CHECK";
        case SystemState::MANUAL_FEED:        return "MANUAL_FEED";
        case SystemState::MANUAL_WATER:       return "MANUAL_WATER";
        case SystemState::ERROR:              return "ERROR";
        case SystemState::EMERGENCY_STOP:     return "EMERGENCY_STOP";
        default:                              return "UNKNOWN";
    }
}

void StateMachine::requestManualFeed()  { _manualFeedRequested  = true; }
void StateMachine::requestManualWater() { _manualWaterRequested = true; }
void StateMachine::requestEmergencyStop() { _emergencyStopRequested = true; }
