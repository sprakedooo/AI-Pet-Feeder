#pragma once
#include <Arduino.h>
#include "../actuators/PumpController.h"
#include "../managers/SensorManager.h"
#include "../config.h"

enum class WaterResult {
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

class WaterManager {
public:
    WaterManager(PumpController& pump, SensorManager& sensors);
    void begin();

    WaterResult refill();
    void emergencyStop();
    const WaterRecord& getLastRecord() const;

    void update();   // Call in main loop — handles safety timeout while pumping

private:
    PumpController& _pump;
    SensorManager&  _sensors;
    WaterRecord     _lastRecord;
    unsigned long   _pumpStartTime;
    bool            _pumping;

    void _stopPump(WaterResult result, WaterRecord& rec);
};
