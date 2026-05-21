#include "FeedingManager.h"

FeedingManager::FeedingManager(ServoController& servo, SensorManager& sensors)
    : _servo(servo), _sensors(sensors), _targetGrams(DEFAULT_TARGET_GRAMS) {
    memset(&_lastRecord, 0, sizeof(FeedingRecord));
}

void FeedingManager::begin() {
    Serial.println("[FeedingManager] Ready.");
}

FeedResult FeedingManager::dispense(float targetGrams) {
    FeedingRecord rec;
    memset(&rec, 0, sizeof(FeedingRecord));
    rec.targetGrams = targetGrams;
    rec.timestamp   = millis();

    // ── Pre-checks ────────────────────────────────────────────────
    float bowlNow = _sensors.loadCell.readWeight();
    rec.bowlBefore = bowlNow;

    Serial.printf("[Feeding] Bowl weight before: %.1fg\n", bowlNow);

    // Anti-overfeeding: bowl is already sufficiently full
    if (bowlNow >= BOWL_FULL_THRESHOLD_GRAMS) {
        Serial.println("[Feeding] Bowl already full — skipping.");
        rec.result = FeedResult::SKIPPED_BOWL_FULL;
        rec.valid  = true;
        _lastRecord = rec;
        return rec.result;
    }

    // Hopper must have food
    if (!_sensors.hopper.hasFeed()) {
        Serial.println("[Feeding] Hopper empty — cannot dispense.");
        rec.result = FeedResult::SKIPPED_HOPPER_EMPTY;
        rec.valid  = true;
        _lastRecord = rec;
        return rec.result;
    }

    // ── Dispense loop with retries ─────────────────────────────────
    rec.result = _dispenseLoop(targetGrams, rec);

    rec.bowlAfter    = _sensors.loadCell.readWeight();
    rec.dispensedGrams = rec.bowlAfter - rec.bowlBefore;
    if (rec.dispensedGrams < 0) rec.dispensedGrams = 0;
    rec.durationMs   = millis() - rec.timestamp;
    rec.valid        = true;

    _lastRecord = rec;

    Serial.printf("[Feeding] Result: %d | Dispensed: %.1fg | Duration: %lums\n",
        (int)rec.result, rec.dispensedGrams, rec.durationMs);

    return rec.result;
}

FeedResult FeedingManager::_dispenseLoop(float targetGrams, FeedingRecord& rec) {
    unsigned long startTime = millis();

    for (int attempt = 0; attempt < MAX_DISPENSE_RETRIES; attempt++) {
        rec.retryCount = attempt;
        Serial.printf("[Feeding] Attempt %d/%d\n", attempt + 1, MAX_DISPENSE_RETRIES);

        float weightBefore = _sensors.loadCell.readWeight();

        // ── Open flap → shake → weight stabilize ──────────────────
        _servo.open();
        delay(DISPENSE_CYCLE_DELAY_MS);

        _servo.shake();          // Anti-jam oscillation

        // ── Wait for food to fall ──────────────────────────────────
        delay(DISPENSE_CYCLE_DELAY_MS);

        // ── Close flap slowly ─────────────────────────────────────
        _servo.closeSlow();
        delay(FEEDING_VERIFY_DELAY_MS);  // Let food settle

        float weightAfter = _sensors.loadCell.readWeight();

        // ── Jam detection ──────────────────────────────────────────
        if (_checkJam(weightBefore, weightAfter)) {
            rec.jamDetected = true;
            Serial.println("[Feeding] Jam detected! Retrying...");

            // Retry: open/shake/close one more time
            _servo.open();
            _servo.shake();
            _servo.closeSlow();
            delay(FEEDING_VERIFY_DELAY_MS);
            weightAfter = _sensors.loadCell.readWeight();
        }

        // ── Check if target reached ────────────────────────────────
        if (weightAfter >= targetGrams || (weightAfter - _lastRecord.bowlBefore) >= targetGrams) {
            return FeedResult::SUCCESS;
        }

        // ── Timeout check ──────────────────────────────────────────
        if (millis() - startTime > DISPENSE_TIMEOUT_MS) {
            Serial.println("[Feeding] Timeout reached.");
            return FeedResult::FAILED_TIMEOUT;
        }

        // Small gap between retry attempts
        delay(1000);
    }

    return FeedResult::FAILED_NO_WEIGHT_CHANGE;
}

bool FeedingManager::_checkJam(float weightBefore, float weightAfter) {
    // If weight barely changed while flap was open, suspect jam
    float delta = weightAfter - weightBefore;
    return (delta < 2.0f);  // Less than 2g change = possible jam
}

const FeedingRecord& FeedingManager::getLastRecord() const {
    return _lastRecord;
}

void FeedingManager::setTargetGrams(float grams) {
    _targetGrams = constrain(grams, 10.0f, 500.0f);
}

float FeedingManager::getTargetGrams() const {
    return _targetGrams;
}
