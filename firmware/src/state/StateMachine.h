#pragma once
#include <Arduino.h>
#include "../managers/SensorManager.h"
#include "../managers/FeedingManager.h"
#include "../managers/WaterManager.h"
#include "../network/FirebaseManager.h"
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

// Simple feeding schedule entry (loaded from Firebase)
struct FeedSchedule {
    bool  enabled;
    int   hour;
    int   minute;
    float portionGrams;
    bool  triggered;   // Prevent double-triggering within same minute
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

    void loadSchedules();   // Fetch schedules from Firebase

private:
    SensorManager&  _sensors;
    FeedingManager& _feeding;
    WaterManager&   _water;
    FirebaseManager& _firebase;
    AIEngine&       _ai;

    SystemState _state;
    SystemState _prevState;

    static const int MAX_SCHEDULES = 6;
    FeedSchedule _schedules[MAX_SCHEDULES];
    int _scheduleCount;

    unsigned long _lastScheduleCheck;
    unsigned long _lastSensorSync;
    unsigned long _lastHeartbeat;
    unsigned long _lastAIRun;
    unsigned long _lastScheduleLoad;

    bool _manualFeedRequested;
    bool _manualWaterRequested;
    bool _emergencyStopRequested;

    // State handlers
    void _handleIdle();
    void _handleCheckingSchedule();
    void _handlePreFeedCheck();
    void _handleDispensing();
    void _handleWaterCheck();
    void _handleManualFeed();
    void _handleManualWater();
    void _handleError();

    bool _isScheduleMatch();
    float _scheduledPortion();
    void  _resetScheduleTriggers(int hour, int minute);
    void  _transition(SystemState next);
    void  _runPeriodicTasks();
    void  _syncToFirebase();
    void  _runAIAnalysis();
    void  _processCommands();
};
