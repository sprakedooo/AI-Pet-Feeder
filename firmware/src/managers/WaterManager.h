#pragma once
#include <Arduino.h>
#include "../actuators/PumpController.h"
#include "../managers/SensorManager.h"
#include "../config.h"

enum class WaterResult {
    PENDING,              // pump is still running
    SUCCESS,
    SKIPPED_BOWL_FULL,
    SKIPPED_RESERVOIR_EMPTY,
    FAILED_TIMEOUT,
    FAILED_DRY_RUN
};

struct WaterRecord {
    bool valid;
    WaterResult result;
    int levelBefore;
    int levelAfter;
    unsigned long pumpDurationMs;
    unsigned long timestamp;
};

// ── Non-blocking water refill ─────────────────────────────────────────────────
//
//  Usage:
//    water.startRefill();               // call once; returns false if pre-checks fail
//    while (!water.isComplete()) {
//        water.update();                // call every loop() — checks level & timeout
//    }
//    WaterResult r = water.getLastRecord().result;
//
class WaterManager {
public:
    WaterManager(PumpController& pump, SensorManager& sensors);
    void begin();

    // Validate pre-conditions and start the pump.
    // Returns false if the refill was skipped (bowl already full / reservoir empty).
    bool startRefill();

    // Advance pump state — call every loop() while isRefilling().
    void update();

    bool isRefilling()  const;  // true while pump is running
    bool isComplete()   const;  // true once done (success, skipped, or failure)

    void emergencyStop();
    const WaterRecord& getLastRecord() const;

private:
    PumpController& _pump;
    SensorManager&  _sensors;
    WaterRecord     _lastRecord;

    unsigned long _pumpStartTime;
    unsigned long _lastLevelCheck;
    bool          _pumping;
    bool          _complete;

    void _stopPump(WaterResult result, int levelAfter);
};
