#include "StateMachine.h"

StateMachine::StateMachine(SensorManager&  sensors,
                            FeedingManager& feeding,
                            WaterManager&   water,
                            FirebaseManager& firebase,
                            AIEngine&        ai)
    : _sensors(sensors), _feeding(feeding), _water(water),
      _firebase(firebase), _ai(ai),
      _state(SystemState::IDLE), _prevState(SystemState::IDLE),
      _scheduleCount(0), _lastCheckedMinute(-1),
      _matchedPortionGrams(DEFAULT_TARGET_GRAMS),
      _lastScheduleCheck(0), _lastSensorSync(0),
      _lastHeartbeat(0), _lastAIRun(0), _lastScheduleLoad(0), _lastSettingsLoad(0),
      _feedingActive(false), _wateringActive(false),
      _manualFeedRequested(false), _manualWaterRequested(false),
      _emergencyStopRequested(false), _resetWifiRequested(false) {
    memset(_schedules, 0, sizeof(_schedules));
}

void StateMachine::begin() {
    _transition(SystemState::IDLE);
    Serial.println("[StateMachine] Started.");
    loadSchedules();
    // Apply calibration on boot so sensors start with the right ADC ranges
    if (_firebase.isReady()) {
        _applySettings(_firebase.getSettings());
    }
}

void StateMachine::update() {
    // Always check for emergency stop first (works even mid-feed or mid-pump
    // because the main loop now returns quickly from every handler).
    if (_emergencyStopRequested) {
        _water.emergencyStop();
        _feeding.cancel();         // close servo and abort any in-progress dispense
        _feedingActive  = false;
        _wateringActive = false;
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
    // Run schedules even when offline — _schedules[] is populated from flash
    // if Firebase was unreachable at boot or during the last reload.
    // Only skip if there are genuinely no schedules at all.
    if (_scheduleCount == 0) {
        _transition(SystemState::WATER_CHECK);
        return;
    }

    if (_isScheduleMatch()) {
        Serial.println("[StateMachine] Schedule matched — starting feeding.");
        _transition(SystemState::PRE_FEED_CHECK);
    } else {
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
    if (data.hopperStatus == HopperStatus::EMPTY || !_sensors.hopper.hasFeed()) {
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
    // ── First entry: kick off the non-blocking dispense ───────────────────────
    if (!_feedingActive) {
        _firebase.pushState("DISPENSING", true, false);
        _feeding.startDispense(_scheduledPortion());
        _feedingActive = true;
        return;
    }

    // ── Subsequent calls: advance the state machine ───────────────────────────
    _feeding.update();

    if (!_feeding.isComplete()) return; // still in progress — come back next loop

    // ── Dispense finished ─────────────────────────────────────────────────────
    _feedingActive = false;
    const FeedingRecord& rec = _feeding.getLastRecord();
    _firebase.pushFeedingLog(rec);
    _firebase.pushState("IDLE", false, false);

    switch (rec.result) {
        case FeedResult::SUCCESS:
            Serial.println("[StateMachine] Feeding SUCCESS.");
            // bowlLeftFull = true means the pet DIDN'T eat (bowl still full after feeding).
            // rec.bowlAfter >= BOWL_FULL_THRESHOLD_GRAMS → bowl was not touched.
            _ai.recordFeeding(rec.dispensedGrams, false,
                              rec.bowlAfter >= BOWL_FULL_THRESHOLD_GRAMS);
            _firebase.sendNotification("feeding_success",
                "Feeding Complete", "Food dispensed successfully.");
            // Run AI immediately after every successful scheduled feed so
            // insights reflect the latest data without waiting for the timer.
            _runAIAnalysis();
            _lastAIRun = millis(); // reset timer so it doesn't double-fire shortly after
            break;

        case FeedResult::FAILED_JAM:
        case FeedResult::FAILED_TIMEOUT:
        case FeedResult::FAILED_NO_WEIGHT_CHANGE:
            Serial.printf("[StateMachine] Feeding FAILED: %d\n", (int)rec.result);
            _firebase.sendNotification("feeding_failed",
                "Feeding Failed", "Could not dispense food. Please check the feeder.");
            _transition(SystemState::ERROR);
            return;

        default:
            break;
    }

    _transition(SystemState::WATER_CHECK);
}

void StateMachine::_handleWaterCheck() {
    // ── First entry: decide if pumping is needed ──────────────────────────────
    if (!_wateringActive) {
        const SensorData& data = _sensors.getData();

        // Low-food hopper notification (independent of water logic)
        if (data.hopperStatus == HopperStatus::FOOD_LOW) {
            _firebase.sendNotification("food_low",
                "Food Level Low",
                "The food hopper is running low. Please refill soon.");
        }

        if (data.waterLevelPercent < WATER_LOW_THRESHOLD) {
            if (!data.reservoirHasWater) {
                Serial.println("[StateMachine] Reservoir empty — cannot refill.");
                _firebase.sendNotification("reservoir_empty",
                    "Reservoir Empty",
                    "The water reservoir is empty. Please refill.");
                _transition(SystemState::IDLE);
                return;
            }

            Serial.printf("[StateMachine] Water low (%d%%) — starting pump.\n",
                          data.waterLevelPercent);
            _firebase.pushState("WATER_PUMPING", false, true);
            _water.startRefill();     // non-blocking start
            _wateringActive = true;
            return;
        }

        // Water level OK — nothing to do.
        _transition(SystemState::IDLE);
        return;
    }

    // ── Subsequent calls: advance the pump state machine ─────────────────────
    _water.update();

    if (!_water.isComplete()) return; // still pumping — return immediately

    // ── Pump finished ─────────────────────────────────────────────────────────
    _wateringActive = false;
    const WaterRecord& rec = _water.getLastRecord();
    _firebase.pushWaterLog(rec);
    _firebase.pushState("IDLE", false, false);

    switch (rec.result) {
        case WaterResult::SUCCESS:
            _firebase.sendNotification("water_refilled",
                "Water Refilled", "Bowl water has been automatically refilled.");
            break;
        case WaterResult::FAILED_TIMEOUT:
            _firebase.sendNotification("water_failed",
                "Water Refill Failed", "Pump timed out. Please check the system.");
            break;
        case WaterResult::FAILED_DRY_RUN:
            _firebase.sendNotification("reservoir_empty",
                "Reservoir Empty", "Reservoir ran dry during refill.");
            break;
        default:
            break;
    }

    _transition(SystemState::IDLE);
}

void StateMachine::_handleManualFeed() {
    // ── First entry ───────────────────────────────────────────────────────────
    if (!_feedingActive) {
        Serial.println("[StateMachine] Manual feed triggered.");
        _firebase.pushState("MANUAL_FEED", true, false);
        DeviceSettings s = _firebase.getSettings();
        _feeding.startDispense(s.targetPortionGrams);
        _feedingActive = true;
        return;
    }

    // ── Advance state machine ─────────────────────────────────────────────────
    _feeding.update();
    if (!_feeding.isComplete()) return;

    // ── Done ──────────────────────────────────────────────────────────────────
    _feedingActive = false;
    const FeedingRecord& rec = _feeding.getLastRecord();
    _firebase.pushFeedingLog(rec);
    _firebase.pushState("IDLE", false, false);

    switch (rec.result) {
        case FeedResult::SUCCESS:
            _firebase.sendNotification("feeding_success",
                "Manual Feed Complete", "Food dispensed manually.");
            // Same post-feed AI trigger for manual feeds.
            _runAIAnalysis();
            _lastAIRun = millis();
            break;
        case FeedResult::SKIPPED_BOWL_FULL:
            _firebase.sendNotification("feeding_skipped",
                "Manual Feed Skipped", "Bowl is already full.");
            break;
        default:
            _firebase.sendNotification("feeding_failed",
                "Manual Feed Failed", "Could not dispense food.");
            break;
    }

    _transition(SystemState::IDLE);
}

void StateMachine::_handleManualWater() {
    // ── First entry ───────────────────────────────────────────────────────────
    if (!_wateringActive) {
        Serial.println("[StateMachine] Manual water triggered.");
        _firebase.pushState("MANUAL_WATER", false, true);
        _water.startRefill();
        _wateringActive = true;
        return;
    }

    // ── Advance state machine ─────────────────────────────────────────────────
    _water.update();
    if (!_water.isComplete()) return;

    // ── Done ──────────────────────────────────────────────────────────────────
    _wateringActive = false;
    const WaterRecord& rec = _water.getLastRecord();
    _firebase.pushWaterLog(rec);
    _firebase.pushState("IDLE", false, false);

    if (rec.result == WaterResult::SUCCESS) {
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

    // Reload calibration settings every 5 minutes (same cadence as schedules)
    if (now - _lastSettingsLoad >= 300000UL) {
        _lastSettingsLoad = now;
        _applySettings(_firebase.getSettings());
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
    if (cmds.reloadSettings) {
        Serial.println("[StateMachine] Reload-settings command — applying calibration now.");
        _applySettings(_firebase.getSettings());
        _lastSettingsLoad = millis();
    }
    if (cmds.reloadSchedules) {
        Serial.println("[StateMachine] Reload-schedules command — loading now.");
        loadSchedules();
        _lastScheduleLoad = millis(); // reset 5-min timer
    }
    if (cmds.reboot) {
        Serial.println("[StateMachine] Reboot command received.");
        delay(1000);
        ESP.restart();
    }
    if (cmds.resetWifi) {
        Serial.println("[StateMachine] Reset-WiFi command received.");
        _resetWifiRequested = true;
    }
}

// ── Schedule Helpers ──────────────────────────────────────────────

bool StateMachine::_isScheduleMatch() {
    int     hour   = _firebase.getCurrentHour();
    int     minute = _firebase.getCurrentMinute();
    int     dow    = _firebase.getCurrentDayOfWeek(); // 0=Sun…6=Sat
    uint8_t dayBit = (1u << dow);

    // Reset all triggered flags each new minute so the same slot can fire
    // again the next day (and not re-fire within the current minute).
    if (minute != _lastCheckedMinute) {
        _lastCheckedMinute = minute;
        for (int i = 0; i < _scheduleCount; i++) {
            _schedules[i].triggered = false;
        }
    }

    for (int i = 0; i < _scheduleCount; i++) {
        if (!_schedules[i].enabled)          continue; // disabled
        if (_schedules[i].triggered)         continue; // already fired this minute
        if (!(_schedules[i].days & dayBit))  continue; // not scheduled for today
        if (_schedules[i].hour   != hour)    continue;
        if (_schedules[i].minute != minute)  continue;

        _schedules[i].triggered = true;
        _matchedPortionGrams    = _schedules[i].portionGrams > 0
                                  ? _schedules[i].portionGrams
                                  : DEFAULT_TARGET_GRAMS;

        Serial.printf("[StateMachine] Schedule match: %02d:%02d dow=%d portion=%.0fg\n",
                      hour, minute, dow, _matchedPortionGrams);
        return true;
    }
    return false;
}

float StateMachine::_scheduledPortion() {
    // _matchedPortionGrams is set by _isScheduleMatch() at the moment of match.
    return (_matchedPortionGrams > 0) ? _matchedPortionGrams : DEFAULT_TARGET_GRAMS;
}

void StateMachine::loadSchedules() {
    if (!_firebase.isReady()) {
        // Offline — use whatever we saved to flash last time we were online.
        Serial.println("[StateMachine] Firebase not ready — loading schedules from flash.");
        _loadSchedulesFromFlash();
        return;
    }

    int loaded = _firebase.getSchedules(_schedules, MAX_SCHEDULES);
    if (loaded > 0) {
        _scheduleCount = loaded;
        Serial.printf("[StateMachine] %d schedule(s) loaded from Firebase.\n", _scheduleCount);
        _saveSchedulesToFlash(); // Keep flash in sync with latest from server
    } else {
        Serial.println("[StateMachine] No schedules in Firebase — trying flash fallback.");
        _loadSchedulesFromFlash();
    }
}

// ── Flash (NVS) helpers ───────────────────────────────────────────────────────
// Namespace "schedules" → up to 10 entries.
// Key scheme per index i: s{i}_h (hour), s{i}_m (minute), s{i}_g (grams),
//                          s{i}_e (enabled), s{i}_d (daysMask)
// All keys ≤ 15 chars — within NVS limit.

void StateMachine::_saveSchedulesToFlash() {
    _prefs.begin("schedules", false); // read-write
    _prefs.putInt("count", _scheduleCount);
    for (int i = 0; i < _scheduleCount; i++) {
        char key[10];
        snprintf(key, sizeof(key), "s%d_h", i); _prefs.putInt(key,   _schedules[i].hour);
        snprintf(key, sizeof(key), "s%d_m", i); _prefs.putInt(key,   _schedules[i].minute);
        snprintf(key, sizeof(key), "s%d_g", i); _prefs.putFloat(key, _schedules[i].portionGrams);
        snprintf(key, sizeof(key), "s%d_e", i); _prefs.putBool(key,  _schedules[i].enabled);
        snprintf(key, sizeof(key), "s%d_d", i); _prefs.putUInt(key,  _schedules[i].days);
    }
    _prefs.end();
    Serial.printf("[StateMachine] %d schedule(s) saved to flash.\n", _scheduleCount);
}

void StateMachine::_loadSchedulesFromFlash() {
    _prefs.begin("schedules", true); // read-only
    int count = _prefs.getInt("count", 0);
    count = min(count, MAX_SCHEDULES);
    for (int i = 0; i < count; i++) {
        char key[10];
        snprintf(key, sizeof(key), "s%d_h", i); _schedules[i].hour          = _prefs.getInt(key,   8);
        snprintf(key, sizeof(key), "s%d_m", i); _schedules[i].minute        = _prefs.getInt(key,   0);
        snprintf(key, sizeof(key), "s%d_g", i); _schedules[i].portionGrams  = _prefs.getFloat(key, DEFAULT_TARGET_GRAMS);
        snprintf(key, sizeof(key), "s%d_e", i); _schedules[i].enabled       = _prefs.getBool(key,  false);
        snprintf(key, sizeof(key), "s%d_d", i); _schedules[i].days          = (uint8_t)_prefs.getUInt(key, 0x7F);
        _schedules[i].triggered = false;
    }
    _prefs.end();
    _scheduleCount = count;
    if (count > 0) {
        Serial.printf("[StateMachine] %d schedule(s) loaded from flash (offline mode).\n", count);
    } else {
        Serial.println("[StateMachine] No schedules in flash yet.");
    }
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

bool StateMachine::shouldResetWifi() {
    if (_resetWifiRequested) {
        _resetWifiRequested = false; // consume — only fires once
        return true;
    }
    return false;
}

void StateMachine::_applySettings(const DeviceSettings& s) {
    // Push calibration values into sensor objects so they use the right ADC ranges.
    _sensors.waterLevel.setCalibration(s.waterEmptyADC,      s.waterFullADC);
    _sensors.floatSensor.setCalibration(s.reservoirEmptyADC, s.reservoirFullADC);
    _sensors.loadCell.setCalibrationFactor(s.calibrationFactor);
    // Also propagate portion target to feeding manager
    _feeding.setTargetGrams(s.targetPortionGrams);
    Serial.printf("[StateMachine] Settings applied: portion=%.0fg water=%d-%d reservoir=%d-%d cell=%.1f\n",
        s.targetPortionGrams,
        s.waterEmptyADC, s.waterFullADC,
        s.reservoirEmptyADC, s.reservoirFullADC,
        s.calibrationFactor);
}
