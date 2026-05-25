#pragma once
#include <Arduino.h>
#include <HX711.h>
#include <Preferences.h>
#include "../config.h"

class LoadCellManager {
public:
    LoadCellManager();
    void begin();
    void tare();                   // Tare + save offset to NVS
    float readWeight();            // Returns weight in grams (averaged)
    float getLastWeight() const;   // Returns cached last reading
    bool isReady();
    bool isInitialized() const;

    // Update calibration factor at runtime (from Firebase calibration settings).
    void setCalibrationFactor(float factor);

private:
    HX711       _scale;
    Preferences _prefs;
    float       _lastWeight;
    bool        _initialized;

    void  _saveOffset(long offset);
    long  _loadOffset();
    float _average(int samples);
};
