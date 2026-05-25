#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "../managers/SensorManager.h"
#include "../managers/FeedingManager.h"
#include "../managers/WaterManager.h"
#include "../network/FirebaseManager.h"
#include "../types/ScheduleEntry.h"
#include "../ai/AIEngine.h"
#include "../config.h"

enum class SystemState {
    IDLE,
    CHECKING_SCHEDULE,
    PRE_FEED_CHECK,
    DISPENSING,
    DISPENSE_VERIFY,
    WATER_CHECK,
    WATER_PUMPING,
    MANUAL_FEED,
    MANUAL_WATER,
    ERROR,
    EMERGENCY_STOP
};

class StateMachine {
public:
    StateMachine(SensorManager&  sensors,
                 FeedingManager& feeding,
                 WaterManager&   water,
                 FirebaseManager& firebase,
                 AIEngine&        ai);

    void begin();
    void update();                            // Call every loop iteration

    SystemState getState() const;
    const char* stateString() const;

    void requestManualFeed();
    void requestManualWater();
    void requestEmergencyStop();

    // Returns true once when a resetWifi command arrives.
    // main.cpp checks this, clears WiFi credentials, then reboots.
    bool shouldResetWifi();

    void loadSchedules();   // Fetch from Firebase; falls back to flash when offline

private:
    SensorManager&  _sensors;
    FeedingManager& _feeding;
    WaterManager&   _water;
    FirebaseManager& _firebase;
    AIEngine&       _ai;

    SystemState _state;
    SystemState _prevState;

    static const int MAX_SCHEDULES = 10;
    FeedSchedule _schedules[MAX_SCHEDULES];
    int   _scheduleCount;
    int   _lastCheckedMinute;    // For per-minute trigger reset
    float _matchedPortionGrams;  // Portion of the last matched schedule

    unsigned long _lastScheduleCheck;
    unsigned long _lastSensorSync;
    unsigned long _lastHeartbeat;
    unsigned long _lastAIRun;
    unsigned long _lastScheduleLoad;
    unsigned long _lastSettingsLoad;

    bool _manualFeedRequested;
    bool _manualWaterRequested;
    bool _emergencyStopRequested;
    bool _resetWifiRequested;

    // Non-blocking operation guards — set true on first entry, cleared on completion.
    bool _feedingActive;
    bool _wateringActive;

    // NVS flash storage for offline schedule persistence
    Preferences _prefs;

    // State handlers
    void _handleIdle();
    void _handleCheckingSchedule();
    void _handlePreFeedCheck();
    void _handleDispensing();
    void _handleWaterCheck();
    void _handleManualFeed();
    void _handleManualWater();
    void _handleError();

    bool  _isScheduleMatch();   // returns true and sets _matchedPortionGrams
    float _scheduledPortion();  // returns _matchedPortionGrams (set at match time)
    void  _transition(SystemState next);
    void  _runPeriodicTasks();
    void  _syncToFirebase();
    void  _runAIAnalysis();
    void  _processCommands();
    void  _applySettings(const DeviceSettings& s);  // Push calibration to sensors

    // Flash (NVS) helpers for offline schedule persistence
    void _saveSchedulesToFlash();    // Call after every successful Firebase load
    void _loadSchedulesFromFlash();  // Fallback when Firebase is unreachable
};
