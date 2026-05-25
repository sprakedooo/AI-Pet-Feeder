#pragma once
#include <Arduino.h>
#include "../actuators/ServoController.h"
#include "../managers/SensorManager.h"
#include "../config.h"

enum class FeedResult {
    PENDING,
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
    float dispensedGrams;   // total added (bowlAfter - bowlBefore)
    float bowlBefore;       // weight measured once before dispensing starts
    float bowlAfter;        // weight after final pulse
    int   pulseCount;       // number of 1-second pulses used
    bool  jamDetected;      // true if any pulse added < 2 g
    unsigned long durationMs;
    unsigned long timestamp;
};

// ── Pulse-based non-blocking feeding state machine ───────────────────────────
//
//  Each "pulse" = open gate 30° → wait 1 s → close → settle → weigh.
//  Pulses repeat until the bowl hits the target weight or a limit is reached.
//
//  Usage:
//    startDispense(grams);
//    while (!feeding.isComplete()) feeding.update();
//    FeedResult r = feeding.getLastRecord().result;
//
class FeedingManager {
public:
    FeedingManager(ServoController& servo, SensorManager& sensors);
    void begin();

    void startDispense(float targetGrams = DEFAULT_TARGET_GRAMS);
    void cancel();
    void update();

    bool isComplete()  const;
    bool isDispensing() const;

    const FeedingRecord& getLastRecord() const;

    void  setTargetGrams(float grams);
    float getTargetGrams() const;

private:
    // ── Phases ───────────────────────────────────────────────────────────────
    //
    //  PRE_PET_WAIT  → wait for pet to leave so bowlBefore isn't inflated
    //  PRE_SETTLE    → 1 s scale damping after pet leaves
    //  PULSE_OPEN    → open gate to SERVO_DISPENSE_ANGLE (30°)  [blocking ~300 ms]
    //  PULSE_FLOW    → non-blocking wait PULSE_OPEN_MS (1 s) for food to fall
    //  PULSE_CLOSE   → snap gate shut instantly
    //  PULSE_SETTLE  → non-blocking wait PULSE_SETTLE_MS (600 ms) for scale
    //  PULSE_WEIGH   → read scale; loop back or finish
    //  DONE          → finished (success or failure)
    //
    enum class Phase : uint8_t {
        IDLE,
        PRE_PET_WAIT,
        PRE_SETTLE,
        PULSE_OPEN,
        PULSE_FLOW,
        PULSE_CLOSE,
        PULSE_SETTLE,
        PULSE_WEIGH,
        DONE
    };

    ServoController& _servo;
    SensorManager&   _sensors;
    FeedingRecord    _lastRecord;
    float            _targetGrams;

    Phase         _phase;
    unsigned long _phaseStart;
    unsigned long _dispenseStart;

    float _bowlBefore;
    float _weightAtPulseStart; // bowl weight at start of current pulse (for per-pulse flow check)
    int   _pulseCount;
    bool  _petPresentPre;

    void _goTo(Phase next);
    void _finish(FeedResult result, float bowlAfter);
};
