#pragma once
#include <Arduino.h>
#include "../config.h"

// ── Insight types ──────────────────────────────────────────────────
enum class InsightType {
    APPETITE_WARNING,
    LOW_APPETITE_ALERT,
    DEHYDRATION_RISK,
    REFILL_PREDICTION,
    SCHEDULE_INCONSISTENCY,
    OVERFEEDING_RISK,
    BEHAVIOR_CHANGE,
    INFO
};

struct AIInsight {
    InsightType type;
    char        title[64];
    char        message[192];
    int         severity;       // 1 (info) – 5 (critical)
    bool        isActionable;
    unsigned long generatedAt;
};

// ── Feeding history snapshot (rolling window) ──────────────────────
struct FeedingSnapshot {
    float dispensedGrams;
    bool  skipped;
    bool  bowlLeftFull;      // bowl was not empty at next feeding
    unsigned long timestamp;
};

// ── Water history snapshot ─────────────────────────────────────────
struct WaterSnapshot {
    int   levelBefore;
    int   levelAfter;
    unsigned long timestamp;
};

class AIEngine {
public:
    AIEngine();
    void begin();

    // Record events for analysis
    void recordFeeding(float grams, bool skipped, bool bowlLeftFull);
    void recordWaterRefill(int levelBefore, int levelAfter);
    void recordHopperEmpty();

    // Run analysis and generate insights
    int  analyze(AIInsight* outputBuffer, int maxInsights);

    // Refill prediction
    int  daysUntilHopperRefill() const;   // -1 = unknown

private:
    static const int HISTORY_SIZE = 14;   // 14 feeding events rolling window

    FeedingSnapshot _feedHistory[HISTORY_SIZE];
    WaterSnapshot   _waterHistory[HISTORY_SIZE];
    int             _feedCount;
    int             _waterCount;

    int  _hopperEmptyEvents;
    unsigned long _lastHopperEmpty;
    unsigned long _firstHopperEmpty;

    // Rule evaluators — each fills up to 1 insight, returns count added
    int _checkAppetite(AIInsight* buf, int idx);
    int _checkWaterConsumption(AIInsight* buf, int idx);
    int _checkSkippedMeals(AIInsight* buf, int idx);
    int _checkRefillPrediction(AIInsight* buf, int idx);
    int _checkOverfeeding(AIInsight* buf, int idx);

    float _avgDispensedGrams() const;
    float _avgWaterDelta() const;
    int   _consecutiveSkipped() const;
};
