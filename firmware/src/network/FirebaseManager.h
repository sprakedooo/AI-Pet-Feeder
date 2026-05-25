#pragma once
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "../managers/SensorManager.h"
#include "../managers/FeedingManager.h"
#include "../managers/WaterManager.h"
#include "../types/ScheduleEntry.h"
#include "../config.h"

struct DeviceSettings {
    float targetPortionGrams;
    int   waterLowThreshold;
    int   detectionDistanceCm;
    bool  autoMode;
    bool  notificationsEnabled;
    // Sensor calibration (debug menu in app → settings_cache/calibration)
    int   waterEmptyADC;
    int   waterFullADC;
    int   reservoirEmptyADC;
    int   reservoirFullADC;
    float calibrationFactor;
};

struct PendingCommands {
    bool manualFeed;
    bool manualWater;
    bool emergencyStop;
    bool tare;
    bool reboot;
    bool reloadSettings;   // app sets after saving calibration → instant apply
    bool reloadSchedules;  // app sets after saving a schedule → instant load
    bool resetWifi;        // app sets to clear NVS credentials and reboot into AP setup mode
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

    // Schedules — reads from settings_cache/schedules in RTDB.
    // Returns the number of schedules loaded (0 on error or empty).
    int getSchedules(FeedSchedule* out, int maxCount);

    // Time
    unsigned long getEpochTime();
    int getCurrentHour();
    int getCurrentMinute();
    int getCurrentDayOfWeek(); // 0=Sunday, 1=Monday, ..., 6=Saturday

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

    // Returns true if the system clock is set to a plausible time (> 2024)
    bool _clockReady();
    // Fallback: sync time via HTTP Date header (works even when UDP 123 is blocked)
    bool _syncTimeViaHTTP();
    void _authenticate();
    String _basePath();
    void _clearCommand(const char* command);
};
