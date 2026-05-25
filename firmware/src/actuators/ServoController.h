#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
#include "../config.h"

class ServoController {
public:
    ServoController();
    void begin();

    // Open to a specific angle (1–45°).  Defaults to fully open (SERVO_OPEN_ANGLE).
    // The angle should be scaled by the caller to the target food portion.
    void open(int angle = SERVO_OPEN_ANGLE);
    void close();
    bool isOpen() const;

private:
    Servo _servo;
    bool  _isOpen;

    void _setAngle(int angle);
};
