#pragma once
#include <Arduino.h>
#include <HX711.h>
#include "../config.h"

class LoadCellManager {
public:
    LoadCellManager();
    void begin();
    void tare();
    float readWeight();          // Returns weight in grams (averaged)
    float getLastWeight() const; // Returns cached last reading
    bool isReady() const;
    bool isInitialized() const;

private:
    HX711 _scale;
    float _lastWeight;
    bool _initialized;

    float _average(int samples);
};
