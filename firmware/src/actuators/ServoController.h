#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
#include "../config.h"

class ServoController {
public:
    ServoController();
    void begin();
    void open();
    void close();
    void closeSlow();     // Gradually close to reduce jam risk
    void shake();         // Anti-jam oscillation
    bool isOpen() const;

private:
    Servo _servo;
    bool _isOpen;

    void _setAngle(int angle);
};
