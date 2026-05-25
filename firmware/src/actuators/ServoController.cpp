#include "ServoController.h"

ServoController::ServoController() : _isOpen(false) {}

void ServoController::begin() {
    // 1. Drive GPIO LOW before servo library takes over — prevents floating
    //    pin from sending a spurious pulse during the bootloader phase.
    pinMode(PIN_SERVO, OUTPUT);
    digitalWrite(PIN_SERVO, LOW);
    delay(50);

    // 2. Set 0° before attach so the library starts PWM at 0° (not 90°).
    _servo.write(SERVO_CLOSE_ANGLE);
    _servo.attach(PIN_SERVO);
    _servo.write(SERVO_CLOSE_ANGLE);

    // 3. Hold 1 s — enough for the servo to travel from any position to 0°.
    delay(1000);

    _isOpen = false;
    Serial.println("[Servo] Initialized at 0 degrees (closed).");
}

void ServoController::open(int angle) {
    angle = constrain(angle, 1, SERVO_OPEN_ANGLE);
    _setAngle(angle);
    _isOpen = true;
    Serial.printf("[Servo] Opened to %d°.\n", angle);
}

void ServoController::close() {
    _setAngle(SERVO_CLOSE_ANGLE);
    _isOpen = false;
    Serial.println("[Servo] Closed.");
}

bool ServoController::isOpen() const {
    return _isOpen;
}

void ServoController::_setAngle(int angle) {
    angle = constrain(angle, 0, 180);
    _servo.write(angle);
    delay(300); // Allow servo to physically reach position
}
