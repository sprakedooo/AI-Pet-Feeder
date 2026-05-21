#include "WaterManager.h"

WaterManager::WaterManager(PumpController& pump, SensorManager& sensors)
    : _pump(pump), _sensors(sensors), _pumpStartTime(0), _pumping(false) {
    memset(&_lastRecord, 0, sizeof(WaterRecord));
}

void WaterManager::begin() {
    Serial.println("[WaterManager] Ready.");
}

WaterResult WaterManager::refill() {
    WaterRecord rec;
    memset(&rec, 0, sizeof(WaterRecord));
    rec.timestamp = millis();

    int currentLevel = _sensors.waterLevel.readPercent();
    rec.levelBefore  = currentLevel;

    Serial.printf("[Water] Bowl level: %d%%\n", currentLevel);

    // Bowl already full — skip
    if (currentLevel >= WATER_FULL_THRESHOLD) {
        rec.result = WaterResult::SKIPPED_BOWL_FULL;
        rec.valid  = true;
        _lastRecord = rec;
        Serial.println("[Water] Bowl already full.");
        return rec.result;
    }

    // Dry-run protection: check reservoir before starting pump
    if (!_sensors.floatSensor.hasWater()) {
        rec.result = WaterResult::SKIPPED_RESERVOIR_EMPTY;
        rec.valid  = true;
        _lastRecord = rec;
        Serial.println("[Water] Reservoir empty — pump NOT started (dry-run prevention).");
        return rec.result;
    }

    // ── Start pump ──────────────────────────────────────────────────
    Serial.println("[Water] Starting pump...");
    _pump.on();
    _pumpStartTime = millis();
    _pumping = true;

    // ── Synchronous refill loop ──────────────────────────────────────
    while (true) {
        delay(PUMP_CHECK_INTERVAL_MS);

        int level = _sensors.waterLevel.readPercent();

        // Bowl reached target level
        if (level >= WATER_FULL_THRESHOLD) {
            _stopPump(WaterResult::SUCCESS, rec);
            rec.levelAfter = level;
            Serial.printf("[Water] Bowl full at %d%%. Pump stopped.\n", level);
            break;
        }

        // Timeout safety
        if (millis() - _pumpStartTime > PUMP_TIMEOUT_MS) {
            _stopPump(WaterResult::FAILED_TIMEOUT, rec);
            rec.levelAfter = level;
            Serial.println("[Water] TIMEOUT — pump force stopped.");
            break;
        }

        // Re-check reservoir during pumping
        if (!_sensors.floatSensor.hasWater()) {
            _stopPump(WaterResult::FAILED_DRY_RUN, rec);
            rec.levelAfter = level;
            Serial.println("[Water] Reservoir ran dry during pumping — stopped.");
            break;
        }
    }

    _pumping = false;
    rec.valid = true;
    _lastRecord = rec;
    return rec.result;
}

void WaterManager::emergencyStop() {
    _pump.off();
    _pumping = false;
    Serial.println("[Water] EMERGENCY STOP.");
}

void WaterManager::update() {
    // Safety: cut pump if it's been running without a proper stop
    if (_pump.isRunning() && !_pumping) {
        _pump.off();
        Serial.println("[Water] Stale pump detected — force stopped.");
    }
}

const WaterRecord& WaterManager::getLastRecord() const {
    return _lastRecord;
}

void WaterManager::_stopPump(WaterResult result, WaterRecord& rec) {
    _pump.off();
    rec.result = result;
    rec.pumpDurationMs = millis() - _pumpStartTime;
}
