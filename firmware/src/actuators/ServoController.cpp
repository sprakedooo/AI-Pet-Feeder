#include "ServoController.h"

ServoController::ServoController() : _isOpen(false) {}

void ServoController::begin() {
    _servo.attach(PIN_SERVO);
    _servo.write(SERVO_CLOSE_ANGLE);
    _isOpen = false;
    Serial.println("[Servo] Initialized at closed position.");
}

void ServoController::open() {
    _setAngle(SERVO_OPEN_ANGLE);
    _isOpen = true;
    delay(SERVO_OPEN_DELAY_MS);
    Serial.println("[Servo] Opened.");
}

void ServoController::close() {
    _setAngle(SERVO_CLOSE_ANGLE);
    _isOpen = false;
    Serial.println("[Servo] Closed.");
}

void ServoController::closeSlow() {
    // Slowly move from open angle to close angle, 1° at a time
    int current = _servo.read();
    int target  = SERVO_CLOSE_ANGLE;
    int step    = (current > target) ? -1 : 1;

    while (current != target) {
        current += step;
        _servo.write(current);
        delay(SERVO_CLOSE_SPEED_MS);
    }
    _isOpen = false;
    Serial.println("[Servo] Closed slowly.");
}

void ServoController::shake() {
    Serial.println("[Servo] Anti-jam shake...");
    for (int i = 0; i < SERVO_SHAKE_CYCLES; i++) {
        _servo.write(SERVO_SHAKE_ANGLE_A);
        delay(SERVO_SHAKE_DELAY_MS);
        _servo.write(SERVO_SHAKE_ANGLE_B);
        delay(SERVO_SHAKE_DELAY_MS);
    }
    // Return to open position after shake
    _servo.write(SERVO_OPEN_ANGLE);
    delay(200);
}

bool ServoController::isOpen() const {
    return _isOpen;
}

void ServoController::_setAngle(int angle) {
    angle = constrain(angle, 0, 180);
    _servo.write(angle);
    delay(300); // Allow servo to reach position
}
