#pragma once
#include <Arduino.h>
#include "../sensors/LoadCellManager.h"
#include "../sensors/WaterLevelSensor.h"
#include "../sensors/UltrasonicSensor.h"
#include "../sensors/HopperSensor.h"
#include "../sensors/FloatSensor.h"
#include "../config.h"

struct SensorData {
    float   bowlWeightGrams;
    int     bowlWeightPercent;   // 0–100 based on max bowl capacity
    int     waterLevelPercent;
    HopperStatus hopperStatus;
    bool    reservoirHasWater;
    bool    petDetected;
    float   petDistanceCm;
    unsigned long timestamp;
};

class SensorManager {
public:
    SensorManager();
    void begin();
    void update();                       // Poll all sensors (call periodically)
    const SensorData& getData() const;
    void tareScale();

    LoadCellManager   loadCell;
    WaterLevelSensor  waterLevel;
    UltrasonicSensor  ultrasonic;
    HopperSensor      hopper;
    FloatSensor       floatSensor;

private:
    SensorData _data;
    unsigned long _lastUpdate;

    int _weightToPercent(float grams);
};
