#include "FloatSensor.h"

FloatSensor::FloatSensor()
    : _lastPercent(100), _lastHasWater(true),
      _emptyADC(RESERVOIR_EMPTY_ADC), _fullADC(RESERVOIR_FULL_ADC) {}

void FloatSensor::begin() {
    // GPIO 33 is ADC1_CH5 — input only, no pinMode needed for ADC use
    Serial.println("[Reservoir] Analog water level sensor initialized (GPIO 33).");
}

int FloatSensor::readPercent() {
    int raw = _readADC();

    // Map raw ADC → 0–100 % of what the sensor can see (sensor-to-top of tank).
    int rawPct = constrain(map(raw, _emptyADC, _fullADC, 0, 100), 0, 100);

    // The sensor is mounted at RESERVOIR_SENSOR_MIN_PCT (50 %) of the tank height.
    // Raw 0 % means water is AT the sensor level  → display 50 %.
    // Raw 100 % means water is at the very top    → display 100 %.
    // Formula: display = MIN_PCT + rawPct × (100 - MIN_PCT) / 100
    _lastPercent  = RESERVOIR_SENSOR_MIN_PCT +
                    (rawPct * (100 - RESERVOIR_SENSOR_MIN_PCT) / 100);

    // hasWater is used for dry-run protection. Sensor submerged → rawPct > 0
    // → display > RESERVOIR_SENSOR_MIN_PCT (50 %), well above RESERVOIR_LOW_THRESHOLD.
    _lastHasWater = (_lastPercent > RESERVOIR_LOW_THRESHOLD);

    Serial.printf("[Reservoir] Raw ADC avg: %d → raw pct: %d%% → display: %d%%\n",
                  raw, rawPct, _lastPercent);

    return _lastPercent;
}

bool FloatSensor::hasWater() {
    // Returns the cached value last set by readPercent().
    // Default is true (constructor) so the pump is never blocked before the
    // first sensor cycle.  SensorManager::update() drives readPercent() every
    // SENSOR_POLL_INTERVAL_MS; call readPercent() directly for a live check.
    return _lastHasWater;
}

int FloatSensor::getLastPercent() const {
    return _lastPercent;
}

bool FloatSensor::getLastState() const {
    return _lastHasWater;
}

void FloatSensor::setCalibration(int emptyADC, int fullADC) {
    if (emptyADC >= 0 && fullADC > emptyADC) {
        _emptyADC = emptyADC;
        _fullADC  = fullADC;
        Serial.printf("[Reservoir] Calibration updated: empty=%d full=%d\n", _emptyADC, _fullADC);
    }
}

int FloatSensor::_readADC() {
    long sum = 0;
    for (int i = 0; i < RESERVOIR_SAMPLE_COUNT; i++) {
        sum += analogRead(PIN_RESERVOIR_SENSOR);
        delay(10);
    }
    return (int)(sum / RESERVOIR_SAMPLE_COUNT);
}
