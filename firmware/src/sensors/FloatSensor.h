#pragma once
#include <Arduino.h>
#include "../config.h"

class FloatSensor {
public:
    FloatSensor();
    void begin();
    bool hasWater();           // true = reservoir has water
    bool getLastState() const;

private:
    bool _lastState;
};
