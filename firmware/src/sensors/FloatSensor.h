#pragma once
#include <Arduino.h>
#include "../config.h"

// Reads an analog water level sensor on the reservoir (refill tank).
// Replaces the original NC mechanical switch — now gives a 0–100% level
// reading, not just a binary has/empty state.
// Uses GPIO 33 (ADC1_CH5) — safe to read while WiFi is active.
class FloatSensor {
public:
    FloatSensor();
    void begin();

    int  readPercent();          // 0–100% reservoir level
    bool hasWater();             // true = reservoir above low threshold (safe to pump)
    int  getLastPercent() const;
    bool getLastState()   const;

    // Update ADC endpoints at runtime (from Firebase calibration settings).
    void setCalibration(int emptyADC, int fullADC);

private:
    int  _lastPercent;
    bool _lastHasWater;
    int  _emptyADC;   // default: RESERVOIR_EMPTY_ADC
    int  _fullADC;    // default: RESERVOIR_FULL_ADC

    int _readADC();
};
