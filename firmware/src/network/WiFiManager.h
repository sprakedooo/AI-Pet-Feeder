#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "../config.h"

class WiFiManager {
public:
    WiFiManager();
    void begin();
    void update();               // Call in main loop — handles reconnection & AP serving
    bool isConnected() const;
    bool isAPMode()    const;
    String getIPAddress() const;
    int    getRSSI()   const;

    // Call from a "reset" button or command to wipe saved credentials
    // and fall back to AP setup mode on next reboot.
    void clearCredentials();

private:
    bool          _connected;
    bool          _apMode;
    unsigned long _lastAttempt;
    int           _retryCount;

    Preferences   _prefs;
    WebServer*    _server;
    DNSServer*    _dns;

    bool _loadCredentials(String& ssid, String& pass);
    void _saveCredentials(const String& ssid, const String& pass);
    void _connectSTA(const String& ssid, const String& pass);
    void _startAP();
    void _serveRoot();
    void _handleSave();
};
