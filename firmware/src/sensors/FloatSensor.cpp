#include "FloatSensor.h"

FloatSensor::FloatSensor() : _lastState(true) {}

void FloatSensor::begin() {
    // NC (Normally Closed) mechanical contact switch + INPUT_PULLUP:
    //   Water PRESENT → switch CLOSED → pin pulled to GND → reads LOW  → hasWater() = true
    //   Water LOW     → switch OPEN   → pin floats HIGH   → reads HIGH → hasWater() = false
    pinMode(PIN_FLOAT_SENSOR, INPUT_PULLUP);
    Serial.println("[Float] Initialized (NC switch: LOW=water present, HIGH=water low).");
}

bool FloatSensor::hasWater() {
    // Debounce: read twice 20 ms apart — both must agree
    bool a = digitalRead(PIN_FLOAT_SENSOR);
    delay(20);
    bool b = digitalRead(PIN_FLOAT_SENSOR);

    if (a == b) {
        // LOW  = switch closed = water IS present
        // HIGH = switch open   = water IS low
        _lastState = (b == LOW);
    }
    return _lastState;
}

bool FloatSensor::getLastState() const {
    return _lastState;
}
