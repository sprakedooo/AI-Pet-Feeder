#pragma once
#include <Arduino.h>
#include "../actuators/ServoController.h"
#include "../managers/SensorManager.h"
#include "../config.h"

enum class FeedResult {
    SUCCESS,
    SKIPPED_BOWL_FULL,
    SKIPPED_HOPPER_EMPTY,
    FAILED_TIMEOUT,
    FAILED_JAM,
    FAILED_NO_WEIGHT_CHANGE
};

struct FeedingRecord {
    bool valid;
    FeedResult result;
    float targetGrams;
    float dispensedGrams;
    float bowlBefore;
    float bowlAfter;
    int retryCount;
    bool jamDetected;
    unsigned long durationMs;
    unsigned long timestamp;
};

class FeedingManager {
public:
    FeedingManager(ServoController& servo, SensorManager& sensors);
    void begin();

    FeedResult dispense(float targetGrams = DEFAULT_TARGET_GRAMS);
    const FeedingRecord& getLastRecord() const;

    void setTargetGrams(float grams);
    float getTargetGrams() const;

private:
    ServoController& _servo;
    SensorManager&   _sensors;
    FeedingRecord    _lastRecord;
    float            _targetGrams;

    FeedResult _dispenseLoop(float targetGrams, FeedingRecord& rec);
    bool _checkJam(float weightBefore, float weightAfter);
};
