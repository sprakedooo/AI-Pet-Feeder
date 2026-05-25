#include "HopperSensor.h"

HopperSensor::HopperSensor() : _lastStatus(HopperStatus::OK) {}

void HopperSensor::begin() {
    pinMode(PIN_HOPPER_LED, OUTPUT);
    digitalWrite(PIN_HOPPER_LED, HOPPER_LED_OFF);  // LED off at startup
    // GPIO 35 is ADC input-only — no pinMode needed
    Serial.println("[Hopper] Initialized.");
}

HopperStatus HopperSensor::read() {
    int ldrValue = _readLDR();

#ifdef HOPPER_LAYOUT_BEAM_BREAK
    // BEAM-BREAK wiring: LDR has HIGH resistance when light hits it (unusual but confirmed).
    // Beam free  (no food)  → LDR lit   → HIGH resistance → HIGH ADC = empty
    // Beam blocked by food  → LDR dark  → LOW  resistance → LOW  ADC = food present
    Serial.printf("[Hopper] Raw LDR ADC: %d (threshold: %d)\n", ldrValue, HOPPER_FOOD_PRESENT_ADC);
    bool foodPresent = (ldrValue < HOPPER_FOOD_PRESENT_ADC);
    bool foodLow     = !foodPresent && (ldrValue < (HOPPER_FOOD_PRESENT_ADC * 3 / 2));
#else
    // REFLECTIVE (pull-up wiring: VCC → LDR → ADC → R → GND)
    // Food reflects LED light to LDR  → LDR lit  → HIGH ADC  = food present
    // No food, no reflection           → LDR dark → LOW  ADC  = empty
    bool foodPresent = (ldrValue >= HOPPER_FOOD_PRESENT_ADC);
    bool foodLow     = !foodPresent && (ldrValue >= (HOPPER_FOOD_PRESENT_ADC / 2));
#endif

    if (foodPresent) {
        _lastStatus = HopperStatus::OK;
    } else if (foodLow) {
        _lastStatus = HopperStatus::FOOD_LOW;
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
        case HopperStatus::FOOD_LOW:   return "low";
        case HopperStatus::EMPTY: return "empty";
        default:                  return "unknown";
    }
}

bool HopperSensor::hasFeed() const {
    return _lastStatus == HopperStatus::OK || _lastStatus == HopperStatus::FOOD_LOW;
}

int HopperSensor::_readLDR() {
    digitalWrite(PIN_HOPPER_LED, HOPPER_LED_ON);  // LED on for measurement
    delay(50);                                     // Let LED stabilise

    long sum = 0;
    for (int i = 0; i < HOPPER_SAMPLE_COUNT; i++) {
        sum += analogRead(PIN_LDR);
        delay(10);
    }

    digitalWrite(PIN_HOPPER_LED, HOPPER_LED_OFF); // LED off after measurement
    return (int)(sum / HOPPER_SAMPLE_COUNT);
}
