#include "SensorManager.h"

SensorManager::SensorManager() : _lastUpdate(0) {
    memset(&_data, 0, sizeof(SensorData));
    _data.reservoirHasWater = true;
    _data.hopperStatus = HopperStatus::OK;
}

void SensorManager::begin() {
    Serial.println("[SensorManager] Initializing sensors...");
    loadCell.begin();
    waterLevel.begin();
    ultrasonic.begin();
    hopper.begin();
    floatSensor.begin();
    Serial.println("[SensorManager] All sensors initialized.");
}

void SensorManager::update() {
    unsigned long now = millis();
    if (now - _lastUpdate < SENSOR_POLL_INTERVAL_MS) return;
    _lastUpdate = now;

    _data.bowlWeightGrams   = loadCell.readWeight();
    _data.bowlWeightPercent = _weightToPercent(_data.bowlWeightGrams);
    _data.waterLevelPercent = waterLevel.readPercent();
    _data.hopperStatus      = hopper.read();
    _data.reservoirHasWater = floatSensor.hasWater();
    _data.petDistanceCm     = ultrasonic.readDistance();
    _data.petDetected       = ultrasonic.isPetDetected();
    _data.timestamp         = now;

    Serial.printf("[Sensors] Bowl:%.1fg(%d%%) Water:%d%% Hopper:%s Reservoir:%s Pet:%s(%.0fcm)\n",
        _data.bowlWeightGrams,
        _data.bowlWeightPercent,
        _data.waterLevelPercent,
        hopper.statusString(),
        _data.reservoirHasWater ? "ok" : "empty",
        _data.petDetected ? "YES" : "no",
        _data.petDistanceCm
    );
}

const SensorData& SensorManager::getData() const {
    return _data;
}

void SensorManager::tareScale() {
    loadCell.tare();
    _data.bowlWeightGrams = 0.0f;
    _data.bowlWeightPercent = 0;
}

int SensorManager::_weightToPercent(float grams) {
    // Assume max bowl capacity = target portion * 2
    float maxGrams = DEFAULT_TARGET_GRAMS * 2.0f;
    int pct = (int)((grams / maxGrams) * 100.0f);
    return constrain(pct, 0, 100);
}
