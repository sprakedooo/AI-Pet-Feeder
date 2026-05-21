#include "LoadCellManager.h"

LoadCellManager::LoadCellManager()
    : _lastWeight(0.0f), _initialized(false) {}

void LoadCellManager::begin() {
    _scale.begin(PIN_HX711_DT, PIN_HX711_SCK);
    _scale.set_scale(LOAD_CELL_CALIBRATION_FACTOR);

    Serial.println("[LoadCell] Taring scale, please remove all weight...");
    delay(2000);
    _scale.tare(LOAD_CELL_TARE_READINGS);
    Serial.println("[LoadCell] Tare complete.");

    _initialized = true;
}

void LoadCellManager::tare() {
    if (!_initialized) return;
    _scale.tare(LOAD_CELL_TARE_READINGS);
    _lastWeight = 0.0f;
    Serial.println("[LoadCell] Re-tared.");
}

float LoadCellManager::readWeight() {
    if (!_initialized || !_scale.is_ready()) {
        Serial.println("[LoadCell] Not ready.");
        return _lastWeight;
    }

    float w = _average(LOAD_CELL_SAMPLE_READINGS);

    // Clamp negatives (noise around zero)
    if (w < 0.5f) w = 0.0f;

    _lastWeight = w;
    return _lastWeight;
}

float LoadCellManager::getLastWeight() const {
    return _lastWeight;
}

bool LoadCellManager::isReady() const {
    return _initialized && _scale.is_ready();
}

bool LoadCellManager::isInitialized() const {
    return _initialized;
}

float LoadCellManager::_average(int samples) {
    float sum = 0.0f;
    int valid = 0;
    for (int i = 0; i < samples; i++) {
        if (_scale.is_ready()) {
            sum += _scale.get_units(1);
            valid++;
        }
        delay(10);
    }
    return (valid > 0) ? (sum / valid) : _lastWeight;
}
