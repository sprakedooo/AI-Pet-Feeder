#include "HopperSensor.h"

HopperSensor::HopperSensor() : _lastStatus(HopperStatus::OK) {}

void HopperSensor::begin() {
    pinMode(PIN_HOPPER_LED, OUTPUT);
    digitalWrite(PIN_HOPPER_LED, LOW);
    // GPIO 35 is ADC input-only — no pinMode needed
    Serial.println("[Hopper] Initialized.");
}

HopperStatus HopperSensor::read() {
    int ldrValue = _readLDR();

#ifdef HOPPER_LAYOUT_BEAM_BREAK
    // BEAM-BREAK: food blocks the beam → LDR dark → HIGH ADC = food present
    //             no food             → beam hits LDR → LOW ADC = empty
    bool foodPresent = (ldrValue >= HOPPER_FOOD_PRESENT_ADC);
    bool foodLow     = (ldrValue >= HOPPER_FOOD_PRESENT_ADC / 2) && !foodPresent;
#else
    // REFLECTIVE: food reflects LED back to LDR → LDR lit → LOW ADC = food present
    //             no food → no reflection → LDR dark → HIGH ADC = empty
    bool foodPresent = (ldrValue < HOPPER_FOOD_PRESENT_ADC);
    bool foodLow     = (ldrValue < HOPPER_FOOD_PRESENT_ADC * 3 / 2) && !foodPresent;
#endif

    if (foodPresent) {
        _lastStatus = HopperStatus::OK;
    } else if (foodLow) {
        _lastStatus = HopperStatus::LOW;
    } else {
        _lastStatus = HopperStatus::EMPTY;
    }

    return _lastStatus;
}

HopperStatus HopperSensor::getLastStatus() const {
    return _lastStatus;
}

const char* HopperSensor::statusString() const {
    switch (_lastStatus) {
        case HopperStatus::OK:    return "ok";
        case HopperStatus::LOW:   return "low";
        case HopperStatus::EMPTY: return "empty";
        default:                  return "unknown";
    }
}

bool HopperSensor::hasFeed() const {
    return _lastStatus == HopperStatus::OK || _lastStatus == HopperStatus::LOW;
}

int HopperSensor::_readLDR() {
    // Turn LED on briefly for consistent reading
    digitalWrite(PIN_HOPPER_LED, HIGH);
    delay(50);

    long sum = 0;
    for (int i = 0; i < HOPPER_SAMPLE_COUNT; i++) {
        sum += analogRead(PIN_LDR);
        delay(10);
    }

    digitalWrite(PIN_HOPPER_LED, LOW);
    return (int)(sum / HOPPER_SAMPLE_COUNT);
}
