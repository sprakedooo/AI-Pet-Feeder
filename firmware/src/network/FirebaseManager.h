#pragma once
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "../managers/SensorManager.h"
#include "../managers/FeedingManager.h"
#include "../managers/WaterManager.h"
#include "../config.h"

struct DeviceSettings {
    float targetPortionGrams;
    int   waterLowThreshold;
    int   detectionDistanceCm;
    bool  autoMode;
    bool  notificationsEnabled;
};

struct PendingCommands {
    bool manualFeed;
    bool manualWater;
    bool emergencyStop;
    bool tare;
    bool reboot;
};

class FirebaseManager {
public:
    FirebaseManager();
    void begin();
    void update();                          // Call in loop

    // Push data to Firebase
    void pushSensorData(const SensorData& data);
    void pushState(const char* state, bool isDispensing, bool isPumping);
    void pushFeedingLog(const FeedingRecord& rec);
    void pushWaterLog(const WaterRecord& rec);
    void pushHeartbeat();
    void sendNotification(const char* type, const char* title, const char* body);

    // Pull from Firebase
    PendingCommands  checkCommands();
    DeviceSettings   getSettings();

    // Time
    unsigned long getEpochTime();
    int getCurrentHour();
    int getCurrentMinute();

    bool isReady() const;

private:
    FirebaseData    _fbdo;
    FirebaseData    _fbdo2;
    FirebaseAuth    _auth;
    FirebaseConfig  _config;
    WiFiUDP         _ntpUDP;
    NTPClient       _timeClient;

    bool _ready;
    unsigned long _lastSync;
    unsigned long _lastHeartbeat;

    void _authenticate();
    String _basePath();
    void _clearCommand(const char* command);
};
