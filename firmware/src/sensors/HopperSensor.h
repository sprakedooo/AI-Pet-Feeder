#pragma once
#include <Arduino.h>
#include "../config.h"

enum class HopperStatus {
    OK,
    LOW,
    EMPTY
};

class HopperSensor {
public:
    HopperSensor();
    void begin();
    HopperStatus read();
    HopperStatus getLastStatus() const;
    const char* statusString() const;
    bool hasFeed() const;

private:
    HopperStatus _lastStatus;

    int _readLDR();
};
