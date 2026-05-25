#pragma once
#include <Arduino.h>
#include "../config.h"

class WaterLevelSensor {
public:
    WaterLevelSensor();
    void begin();
    int  readPercent();           // Returns 0–100
    int  getLastPercent() const;
    bool isLow() const;
    bool isEmpty() const;

    // Update ADC endpoints at runtime (from Firebase calibration settings).
    void setCalibration(int emptyADC, int fullADC);

private:
    int _lastPercent;
    int _emptyADC;   // default: WATER_EMPTY_ADC
    int _fullADC;    // default: WATER_FULL_ADC

    int _adcToPercent(int raw);
};
