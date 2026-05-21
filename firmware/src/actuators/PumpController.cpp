#include "PumpController.h"

PumpController::PumpController() : _running(false), _startTime(0) {}

void PumpController::begin() {
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, RELAY_OFF);  // Ensure pump is OFF at boot
    _running = false;
    Serial.println("[Pump] Initialized. Relay OFF.");
}

void PumpController::on() {
    if (_running) return;
    digitalWrite(PIN_RELAY, RELAY_ON);
    _running = true;
    _startTime = millis();
    Serial.println("[Pump] ON.");
}

void PumpController::off() {
    if (!_running) return;
    digitalWrite(PIN_RELAY, RELAY_OFF);
    _running = false;
    Serial.printf("[Pump] OFF. Ran for %lums.\n", runDuration());
}

bool PumpController::isRunning() const {
    return _running;
}

unsigned long PumpController::runDuration() const {
    if (!_running) return 0;
    return millis() - _startTime;
}
