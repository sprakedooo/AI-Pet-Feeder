#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "../config.h"

class WiFiManager {
public:
    WiFiManager();
    void begin();
    void update();               // Call in main loop — handles reconnection
    bool isConnected() const;
    String getIPAddress() const;
    int getRSSI() const;

private:
    bool _connected;
    unsigned long _lastAttempt;
    int _retryCount;

    void _connect();
};
