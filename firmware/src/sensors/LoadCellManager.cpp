#include "LoadCellManager.h"

static const char* NVS_NS  = "loadcell";
static const char* NVS_KEY = "offset";

LoadCellManager::LoadCellManager()
    : _lastWeight(0.0f), _initialized(false) {}

void LoadCellManager::begin() {
    _scale.begin(PIN_HX711_DT, PIN_HX711_SCK);
    _scale.set_scale(LOAD_CELL_CALIBRATION_FACTOR);

    // Restore the tare offset that was saved the last time tare() was called.
    // If no offset has ever been saved (first boot), the raw offset stays at 0
    // and weights will be off until the user tares manually from the app.
    long saved = _loadOffset();
    if (saved != 0) {
        _scale.set_offset(saved);
        Serial.printf("[LoadCell] Restored tare offset: %ld\n", saved);
    } else {
        Serial.println("[LoadCell] No saved tare offset — tare via app before use.");
    }

    _initialized = true;
    Serial.println("[LoadCell] Ready. Calibration factor: " +
                   String(LOAD_CELL_CALIBRATION_FACTOR));
}

void LoadCellManager::tare() {
    if (!_initialized) return;
    Serial.println("[LoadCell] Taring — keep bowl empty...");
    _scale.tare(LOAD_CELL_TARE_READINGS);
    _lastWeight = 0.0f;

    // Persist the new offset so the next reboot doesn't need a tare.
    long offset = _scale.get_offset();
    _saveOffset(offset);
    Serial.printf("[LoadCell] Tare complete. Offset %ld saved to NVS.\n", offset);
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

bool LoadCellManager::isReady() {
    return _initialized && _scale.is_ready();
}

bool LoadCellManager::isInitialized() const {
    return _initialized;
}

void LoadCellManager::setCalibrationFactor(float factor) {
    if (factor != 0.0f) {
        _scale.set_scale(factor);
        Serial.printf("[LoadCell] Calibration factor updated: %.2f\n", factor);
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void LoadCellManager::_saveOffset(long offset) {
    _prefs.begin(NVS_NS, false);
    _prefs.putLong(NVS_KEY, offset);
    _prefs.end();
}

long LoadCellManager::_loadOffset() {
    _prefs.begin(NVS_NS, true); // read-only
    long offset = _prefs.getLong(NVS_KEY, 0L);
    _prefs.end();
    return offset;
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
