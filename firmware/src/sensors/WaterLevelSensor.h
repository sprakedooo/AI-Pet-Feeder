#pragma once
#include <Arduino.h>
#include "../config.h"

class WaterLevelSensor {
public:
    WaterLevelSensor();
    void begin();
    int readPercent();           // Returns 0–100
    int getLastPercent() const;
    bool isLow() const;
    bool isEmpty() const;

private:
    int _lastPercent;

    int _adcToPercent(int raw);
};
