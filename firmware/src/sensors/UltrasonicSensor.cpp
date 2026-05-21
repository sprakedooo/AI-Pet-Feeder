#include "UltrasonicSensor.h"

UltrasonicSensor::UltrasonicSensor() : _lastDistance(999.0f) {}

void UltrasonicSensor::begin() {
    pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_ECHO, INPUT);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    Serial.println("[Ultrasonic] Initialized.");
}

float UltrasonicSensor::readDistance() {
    // Send 10µs trigger pulse
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);

    // Measure echo duration
    long duration = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, ULTRASONIC_TIMEOUT_US);

    if (duration == 0) {
        // Timeout — no echo received
        _lastDistance = -1.0f;
        return _lastDistance;
    }

    // Distance = (duration / 2) * speed of sound (0.0343 cm/µs)
    _lastDistance = (duration / 2.0f) * 0.0343f;
    return _lastDistance;
}

float UltrasonicSensor::getLastDistance() const {
    return _lastDistance;
}

bool UltrasonicSensor::isPetDetected() const {
    return (_lastDistance > 0 && _lastDistance < PET_DETECTION_DISTANCE_CM);
}
