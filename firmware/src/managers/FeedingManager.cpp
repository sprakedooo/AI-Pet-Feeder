#include "FeedingManager.h"

FeedingManager::FeedingManager(ServoController& servo, SensorManager& sensors)
    : _servo(servo), _sensors(sensors), _targetGrams(DEFAULT_TARGET_GRAMS),
      _phase(Phase::IDLE), _phaseStart(0), _dispenseStart(0),
      _bowlBefore(0.0f), _weightAtPulseStart(0.0f),
      _pulseCount(0), _petPresentPre(false) {
    memset(&_lastRecord, 0, sizeof(FeedingRecord));
}

void FeedingManager::begin() {
    Serial.println("[FeedingManager] Ready.");
}

// ── Public API ────────────────────────────────────────────────────────────────

void FeedingManager::startDispense(float targetGrams) {
    if (_phase != Phase::IDLE && _phase != Phase::DONE) return;

    memset(&_lastRecord, 0, sizeof(FeedingRecord));
    _lastRecord.result      = FeedResult::PENDING;
    _lastRecord.targetGrams = targetGrams;
    _lastRecord.timestamp   = millis();
    _dispenseStart          = millis();
    _pulseCount             = 0;
    _petPresentPre          = false;
    _targetGrams            = constrain(targetGrams, 1.0f, 500.0f);

    Serial.printf("[Feeding] startDispense: target=%.1fg\n", targetGrams);
    _goTo(Phase::PRE_PET_WAIT);
}

// ── State machine ─────────────────────────────────────────────────────────────

void FeedingManager::update() {
    if (_phase == Phase::IDLE || _phase == Phase::DONE) return;

    unsigned long now = millis();

    switch (_phase) {

    // ── PRE-WEIGH: wait for pet to clear the bowl ─────────────────────────────
    case Phase::PRE_PET_WAIT: {
        float dist = _sensors.ultrasonic.readDistance();
        bool  near = (dist > 0.0f && dist < PET_DETECTION_DISTANCE_CM);
        if (near) {
            _petPresentPre = true;
            if (now - _phaseStart < PET_WEIGHT_WAIT_MS) return;
            Serial.println("[Feeding] Pet still present — proceeding anyway.");
        }
        _goTo(Phase::PRE_SETTLE);
        return;
    }

    case Phase::PRE_SETTLE:
        // Give 1 s of damping if a pet was present, otherwise skip.
        if (_petPresentPre && (now - _phaseStart < 1000UL)) return;
        {
            _bowlBefore               = _sensors.loadCell.readWeight();
            _weightAtPulseStart       = _bowlBefore;
            _lastRecord.bowlBefore    = _bowlBefore;

            Serial.printf("[Feeding] Bowl before: %.1fg  target: %.1fg\n",
                          _bowlBefore, _targetGrams);

            if (_bowlBefore >= BOWL_FULL_THRESHOLD_GRAMS) {
                Serial.println("[Feeding] Bowl already full — skipping.");
                _finish(FeedResult::SKIPPED_BOWL_FULL, _bowlBefore);
                return;
            }
            if (!_sensors.hopper.hasFeed()) {
                Serial.println("[Feeding] Hopper empty — cannot dispense.");
                _finish(FeedResult::SKIPPED_HOPPER_EMPTY, _bowlBefore);
                return;
            }

            Serial.println("[Feeding] Starting pulse dispensing.");
            _goTo(Phase::PULSE_OPEN);
        }
        return;

    // ── PULSE CYCLE ───────────────────────────────────────────────────────────

    case Phase::PULSE_OPEN:
        // Open gate to fixed 30°.  _setAngle inside open() takes ~300 ms.
        Serial.printf("[Feeding] Pulse %d — opening gate to %d°.\n",
                      _pulseCount + 1, SERVO_DISPENSE_ANGLE);
        _servo.open(SERVO_DISPENSE_ANGLE);
        _goTo(Phase::PULSE_FLOW);
        return;

    case Phase::PULSE_FLOW:
        // Non-blocking: keep gate open for PULSE_OPEN_MS so food flows through.
        if (now - _phaseStart < PULSE_OPEN_MS) return;
        _goTo(Phase::PULSE_CLOSE);
        return;

    case Phase::PULSE_CLOSE:
        // Snap gate shut instantly — fast close keeps weighing accurate.
        _servo.close();
        _goTo(Phase::PULSE_SETTLE);
        return;

    case Phase::PULSE_SETTLE:
        // Non-blocking: let pellets settle on the scale before reading.
        if (now - _phaseStart < PULSE_SETTLE_MS) return;
        _goTo(Phase::PULSE_WEIGH);
        return;

    case Phase::PULSE_WEIGH: {
        float weightNow      = _sensors.loadCell.readWeight();
        float addedThisPulse = weightNow - _weightAtPulseStart;
        float totalAdded     = weightNow - _bowlBefore;

        Serial.printf("[Feeding] Pulse %d done: +%.1fg this pulse | +%.1fg total | bowl=%.1fg\n",
                      _pulseCount + 1, addedThisPulse, totalAdded, weightNow);

        // Low-flow flag (< 2 g in a pulse → possible jam or empty hopper)
        if (addedThisPulse < 2.0f) {
            _lastRecord.jamDetected = true;
            Serial.println("[Feeding] Low flow detected on this pulse.");
        }

        // ── Target reached ────────────────────────────────────────────────
        if (weightNow >= _targetGrams || totalAdded >= _targetGrams) {
            Serial.printf("[Feeding] Target reached after %d pulse(s).\n", _pulseCount + 1);
            _finish(FeedResult::SUCCESS, weightNow);
            return;
        }

        // ── Overall timeout ───────────────────────────────────────────────
        if (now - _dispenseStart >= DISPENSE_TIMEOUT_MS) {
            Serial.println("[Feeding] Dispense timeout.");
            _lastRecord.dispensedGrams = totalAdded > 0.0f ? totalAdded : 0.0f;
            _finish(FeedResult::FAILED_TIMEOUT, weightNow);
            return;
        }

        // ── Max pulses ────────────────────────────────────────────────────
        if (_pulseCount + 1 >= MAX_PULSES) {
            Serial.printf("[Feeding] Max pulses (%d) reached.\n", MAX_PULSES);
            _lastRecord.dispensedGrams = totalAdded > 0.0f ? totalAdded : 0.0f;
            _finish(_lastRecord.jamDetected ? FeedResult::FAILED_JAM
                                            : FeedResult::FAILED_NO_WEIGHT_CHANGE,
                    weightNow);
            return;
        }

        // ── Next pulse ────────────────────────────────────────────────────
        _pulseCount++;
        _lastRecord.pulseCount     = _pulseCount;
        _weightAtPulseStart        = weightNow;
        _goTo(Phase::PULSE_OPEN);
        return;
    }

    default:
        break;
    }
}

void FeedingManager::cancel() {
    if (_phase != Phase::IDLE) {
        _servo.close();
        _phase = Phase::IDLE;
        Serial.println("[FeedingManager] Cancelled.");
    }
}

bool FeedingManager::isComplete()  const { return _phase == Phase::DONE; }
bool FeedingManager::isDispensing() const {
    return _phase != Phase::IDLE && _phase != Phase::DONE;
}

const FeedingRecord& FeedingManager::getLastRecord() const { return _lastRecord; }

void FeedingManager::setTargetGrams(float grams) {
    _targetGrams = constrain(grams, 10.0f, 500.0f);
}
float FeedingManager::getTargetGrams() const { return _targetGrams; }

// ── Private helpers ───────────────────────────────────────────────────────────

void FeedingManager::_goTo(Phase next) {
    _phase      = next;
    _phaseStart = millis();
}

void FeedingManager::_finish(FeedResult result, float bowlAfter) {
    // Safety net: guarantee servo is closed no matter what path brought us here.
    if (_servo.isOpen()) {
        Serial.println("[Feeding] _finish: servo still open — forcing close.");
        _servo.close();
    }

    _lastRecord.result     = result;
    _lastRecord.bowlAfter  = bowlAfter;
    _lastRecord.pulseCount = _pulseCount + 1;
    if (_lastRecord.dispensedGrams == 0.0f) {
        float d = bowlAfter - _bowlBefore;
        _lastRecord.dispensedGrams = d > 0.0f ? d : 0.0f;
    }
    _lastRecord.durationMs = millis() - _lastRecord.timestamp;
    _lastRecord.valid      = true;

    const char* rstr = "?";
    switch (result) {
        case FeedResult::SUCCESS:                 rstr = "SUCCESS"; break;
        case FeedResult::SKIPPED_BOWL_FULL:       rstr = "SKIPPED_FULL"; break;
        case FeedResult::SKIPPED_HOPPER_EMPTY:    rstr = "SKIPPED_HOPPER_EMPTY"; break;
        case FeedResult::FAILED_TIMEOUT:          rstr = "FAILED_TIMEOUT"; break;
        case FeedResult::FAILED_JAM:              rstr = "FAILED_JAM"; break;
        case FeedResult::FAILED_NO_WEIGHT_CHANGE: rstr = "FAILED_NO_CHANGE"; break;
        default: break;
    }
    Serial.printf("[Feeding] Finished: %s | dispensed=%.1fg | pulses=%d | %lu ms\n",
                  rstr, _lastRecord.dispensedGrams, _lastRecord.pulseCount,
                  _lastRecord.durationMs);

    _phase = Phase::DONE;
}
