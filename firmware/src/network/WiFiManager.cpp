#include "WiFiManager.h"

WiFiManager::WiFiManager()
    : _connected(false), _lastAttempt(0), _retryCount(0) {}

void WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    _connect();
}

void WiFiManager::update() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!_connected) {
            _connected   = true;
            _retryCount  = 0;
            Serial.printf("[WiFi] Reconnected. IP: %s\n", WiFi.localIP().toString().c_str());
        }
        return;
    }

    _connected = false;

    unsigned long now = millis();
    if (now - _lastAttempt < WIFI_RECONNECT_DELAY_MS) return;
    _lastAttempt = now;

    if (_retryCount < WIFI_MAX_RETRIES) {
        Serial.printf("[WiFi] Reconnecting... attempt %d/%d\n", _retryCount + 1, WIFI_MAX_RETRIES);
        WiFi.reconnect();
        _retryCount++;
    } else {
        Serial.println("[WiFi] Max retries reached. Restarting ESP32...");
        delay(1000);
        ESP.restart();
    }
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIPAddress() const {
    return WiFi.localIP().toString();
}

int WiFiManager::getRSSI() const {
    return WiFi.RSSI();
}

void WiFiManager::_connect() {
    Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        _connected = true;
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] Initial connection failed. Will retry in loop.");
    }
}
