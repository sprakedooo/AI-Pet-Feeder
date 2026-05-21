#include "WaterLevelSensor.h"

WaterLevelSensor::WaterLevelSensor() : _lastPercent(0) {}

void WaterLevelSensor::begin() {
    // ADC pin — no pinMode needed (input-only GPIO 34)
    Serial.println("[WaterSensor] Initialized.");
}

int WaterLevelSensor::readPercent() {
    // Average multiple readings to reduce noise
    long sum = 0;
    const int samples = 5;
    for (int i = 0; i < samples; i++) {
        sum += analogRead(PIN_WATER_LEVEL);
        delay(10);
    }
    int raw = sum / samples;
    _lastPercent = _adcToPercent(raw);
    return _lastPercent;
}

int WaterLevelSensor::getLastPercent() const {
    return _lastPercent;
}

bool WaterLevelSensor::isLow() const {
    return _lastPercent < WATER_LOW_THRESHOLD;
}

bool WaterLevelSensor::isEmpty() const {
    return _lastPercent < 5;
}

int WaterLevelSensor::_adcToPercent(int raw) {
    raw = constrain(raw, WATER_EMPTY_ADC, WATER_FULL_ADC);
    return map(raw, WATER_EMPTY_ADC, WATER_FULL_ADC, 0, 100);
}
