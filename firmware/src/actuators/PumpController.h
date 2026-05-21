#pragma once
#include <Arduino.h>
#include "../config.h"

class PumpController {
public:
    PumpController();
    void begin();
    void on();
    void off();
    bool isRunning() const;
    unsigned long runDuration() const;   // ms since pump started

private:
    bool _running;
    unsigned long _startTime;
};
