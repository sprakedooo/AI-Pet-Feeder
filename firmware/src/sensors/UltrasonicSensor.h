#pragma once
#include <Arduino.h>
#include "../config.h"

class UltrasonicSensor {
public:
    UltrasonicSensor();
    void begin();
    float readDistance();           // Returns distance in cm (-1 = error)
    float getLastDistance() const;
    bool isPetDetected() const;

private:
    float _lastDistance;
};
