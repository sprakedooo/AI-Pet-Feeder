#include "WaterManager.h"

WaterManager::WaterManager(PumpController& pump, SensorManager& sensors)
    : _pump(pump), _sensors(sensors),
      _pumpStartTime(0), _lastLevelCheck(0),
      _pumping(false), _complete(true) {
    memset(&_lastRecord, 0, sizeof(WaterRecord));
}

void WaterManager::begin() {
    Serial.println("[WaterManager] Ready.");
}

// ── Non-blocking API ──────────────────────────────────────────────────────────

bool WaterManager::startRefill() {
    if (_pumping) return true; // already running — let update() handle it
    _complete = false;         // reset from previous completed run

    memset(&_lastRecord, 0, sizeof(WaterRecord));
    _lastRecord.timestamp = millis();

    int level = _sensors.waterLevel.readPercent();
    _lastRecord.levelBefore = level;

    Serial.printf("[Water] Bowl level: %d%%\n", level);

    // Bowl already full — nothing to do.
    if (level >= WATER_FULL_THRESHOLD) {
        Serial.println("[Water] Bowl already full — skipping.");
        _stopPump(WaterResult::SKIPPED_BOWL_FULL, level);
        return false;
    }

    // Dry-run protection: refuse to start pump if reservoir is empty.
    if (!_sensors.floatSensor.hasWater()) {
        Serial.println("[Water] Reservoir empty — pump NOT started (dry-run protection).");
        _stopPump(WaterResult::SKIPPED_RESERVOIR_EMPTY, level);
        return false;
    }

    // Start pump.
    Serial.printf("[Water] Starting pump... (target: %d%%)\n", WATER_FULL_THRESHOLD);
    _pump.on();
    _pumpStartTime  = millis();
    _lastLevelCheck = millis();
    _pumping        = true;
    return true;
}

// Call every loop() iteration while isRefilling() is true.
// Checks bowl level and timeout without blocking.
void WaterManager::update() {
    // Safety watchdog: if pump is physically running but we lost track of it, cut it.
    if (_pump.isRunning() && !_pumping) {
        _pump.off();
        Serial.println("[Water] Stale pump detected — force stopped.");
        return;
    }

    if (!_pumping) return;

    unsigned long now = millis();

    // Only sample the sensor every PUMP_CHECK_INTERVAL_MS to avoid ADC noise
    // from being sampled too rapidly while the motor is on.
    if (now - _lastLevelCheck < PUMP_CHECK_INTERVAL_MS) return;
    _lastLevelCheck = now;

    int level = _sensors.waterLevel.readPercent();

    // ── Stop conditions (checked in priority order) ───────────────────────────

    // 1. Bowl reached target → success.
    if (level >= WATER_FULL_THRESHOLD) {
        Serial.printf("[Water] Bowl full at %d%% — pump stopped.\n", level);
        _stopPump(WaterResult::SUCCESS, level);
        return;
    }

    // 2. Overall timeout → stop and report.
    if (now - _pumpStartTime >= PUMP_TIMEOUT_MS) {
        Serial.println("[Water] Pump timeout — force stopped.");
        _stopPump(WaterResult::FAILED_TIMEOUT, level);
        return;
    }

    // 3. Reservoir drained during pumping → dry-run protection.
    // Do a live re-read (not cached) because the reservoir level drops fast.
    if (_sensors.floatSensor.readPercent() <= RESERVOIR_LOW_THRESHOLD) {
        Serial.println("[Water] Reservoir ran dry mid-pump — stopped.");
        _stopPump(WaterResult::FAILED_DRY_RUN, level);
        return;
    }

    // Still pumping — no action needed this tick.
}

bool WaterManager::isRefilling()  const { return _pumping; }
bool WaterManager::isComplete()   const { return _complete; }

void WaterManager::emergencyStop() {
    _pump.off();
    _pumping  = false;
    _complete = true;
    Serial.println("[Water] EMERGENCY STOP.");
}

const WaterRecord& WaterManager::getLastRecord() const { return _lastRecord; }

// ── Private ───────────────────────────────────────────────────────────────────

void WaterManager::_stopPump(WaterResult result, int levelAfter) {
    _pump.off();
    _lastRecord.result         = result;
    _lastRecord.levelAfter     = levelAfter;
    _lastRecord.pumpDurationMs = (_pumping) ? millis() - _pumpStartTime : 0;
    _lastRecord.valid          = true;
    _pumping                   = false;
    _complete                  = true;
}
