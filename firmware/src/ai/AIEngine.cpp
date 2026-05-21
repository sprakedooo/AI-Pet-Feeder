#include "AIEngine.h"
#include <string.h>

AIEngine::AIEngine()
    : _feedCount(0), _waterCount(0),
      _hopperEmptyEvents(0), _lastHopperEmpty(0), _firstHopperEmpty(0) {
    memset(_feedHistory,  0, sizeof(_feedHistory));
    memset(_waterHistory, 0, sizeof(_waterHistory));
}

void AIEngine::begin() {
    Serial.println("[AI] Rule-based engine initialized.");
}

void AIEngine::recordFeeding(float grams, bool skipped, bool bowlLeftFull) {
    int idx = _feedCount % HISTORY_SIZE;
    _feedHistory[idx].dispensedGrams = grams;
    _feedHistory[idx].skipped        = skipped;
    _feedHistory[idx].bowlLeftFull   = bowlLeftFull;
    _feedHistory[idx].timestamp      = millis();
    _feedCount++;
}

void AIEngine::recordWaterRefill(int levelBefore, int levelAfter) {
    int idx = _waterCount % HISTORY_SIZE;
    _waterHistory[idx].levelBefore = levelBefore;
    _waterHistory[idx].levelAfter  = levelAfter;
    _waterHistory[idx].timestamp   = millis();
    _waterCount++;
}

void AIEngine::recordHopperEmpty() {
    if (_hopperEmptyEvents == 0) _firstHopperEmpty = millis();
    _lastHopperEmpty = millis();
    _hopperEmptyEvents++;
}

int AIEngine::analyze(AIInsight* buf, int maxInsights) {
    int count = 0;
    if (_feedCount < 3) return 0;  // Need at least 3 events to analyze

    count += _checkAppetite(buf, count);
    if (count < maxInsights) count += _checkSkippedMeals(buf, count);
    if (count < maxInsights) count += _checkWaterConsumption(buf, count);
    if (count < maxInsights) count += _checkRefillPrediction(buf, count);
    if (count < maxInsights) count += _checkOverfeeding(buf, count);

    Serial.printf("[AI] Analysis complete. %d insight(s) generated.\n", count);
    return count;
}

// ── Rule: Appetite Warning ─────────────────────────────────────────
int AIEngine::_checkAppetite(AIInsight* buf, int idx) {
    float recent = _avgDispensedGrams();
    if (recent < 0) return 0;

    // If recent avg is 40% below the target portion, flag it
    float threshold = DEFAULT_TARGET_GRAMS * 0.6f;
    if (recent < threshold && recent > 0) {
        buf[idx].type        = InsightType::LOW_APPETITE_ALERT;
        buf[idx].severity    = 3;
        buf[idx].isActionable = true;
        buf[idx].generatedAt = millis();
        snprintf(buf[idx].title, sizeof(buf[idx].title),
                 "Reduced Appetite Detected");
        snprintf(buf[idx].message, sizeof(buf[idx].message),
                 "Your pet has been eating %.0fg on average, which is %.0f%% below the target of %.0fg. Consider checking on your pet.",
                 recent,
                 (1.0f - recent / DEFAULT_TARGET_GRAMS) * 100.0f,
                 (float)DEFAULT_TARGET_GRAMS);
        return 1;
    }
    return 0;
}

// ── Rule: Consecutive Skipped Meals ───────────────────────────────
int AIEngine::_checkSkippedMeals(AIInsight* buf, int idx) {
    int skipped = _consecutiveSkipped();
    if (skipped >= 2) {
        buf[idx].type        = InsightType::SCHEDULE_INCONSISTENCY;
        buf[idx].severity    = skipped >= 3 ? 4 : 2;
        buf[idx].isActionable = true;
        buf[idx].generatedAt = millis();
        snprintf(buf[idx].title, sizeof(buf[idx].title),
                 "%d Meals Skipped in a Row", skipped);
        snprintf(buf[idx].message, sizeof(buf[idx].message),
                 "The bowl was still full for %d consecutive scheduled feedings. Your pet may not be eating, or the bowl threshold needs adjustment.",
                 skipped);
        return 1;
    }
    return 0;
}

// ── Rule: Water Consumption ───────────────────────────────────────
int AIEngine::_checkWaterConsumption(AIInsight* buf, int idx) {
    if (_waterCount < 3) return 0;

    float avgDelta = _avgWaterDelta();
    // avgDelta ≈ how much water drops between refills (consumption indicator)
    // Low consumption could mean dehydration or blocked water sensor
    if (avgDelta < 5) {  // Less than 5% consumption between checks
        buf[idx].type        = InsightType::DEHYDRATION_RISK;
        buf[idx].severity    = 3;
        buf[idx].isActionable = true;
        buf[idx].generatedAt = millis();
        snprintf(buf[idx].title, sizeof(buf[idx].title),
                 "Low Water Consumption");
        snprintf(buf[idx].message, sizeof(buf[idx].message),
                 "Water levels have not been dropping significantly. Your pet may not be drinking enough, or the water sensor needs calibration.");
        return 1;
    }
    return 0;
}

// ── Rule: Hopper Refill Prediction ───────────────────────────────
int AIEngine::_checkRefillPrediction(AIInsight* buf, int idx) {
    if (_hopperEmptyEvents < 2) return 0;

    int days = daysUntilHopperRefill();
    if (days >= 0 && days <= 3) {
        buf[idx].type        = InsightType::REFILL_PREDICTION;
        buf[idx].severity    = days == 0 ? 4 : 2;
        buf[idx].isActionable = true;
        buf[idx].generatedAt = millis();
        snprintf(buf[idx].title, sizeof(buf[idx].title),
                 "Hopper Refill Due in ~%d Day%s", days, days == 1 ? "" : "s");
        snprintf(buf[idx].message, sizeof(buf[idx].message),
                 "Based on your pet's feeding pattern, the food hopper will need refilling in approximately %d day%s.",
                 days, days == 1 ? "" : "s");
        return 1;
    }
    return 0;
}

// ── Rule: Overfeeding Risk ────────────────────────────────────────
int AIEngine::_checkOverfeeding(AIInsight* buf, int idx) {
    // Count how many recent feedings had "bowl left full"
    int total  = min(_feedCount, HISTORY_SIZE);
    int leftFull = 0;
    for (int i = 0; i < total; i++) {
        if (_feedHistory[i].bowlLeftFull) leftFull++;
    }

    float ratio = (float)leftFull / total;
    if (ratio > 0.5f && total >= 4) {  // >50% of feedings left bowl full
        buf[idx].type        = InsightType::OVERFEEDING_RISK;
        buf[idx].severity    = 2;
        buf[idx].isActionable = true;
        buf[idx].generatedAt = millis();
        snprintf(buf[idx].title, sizeof(buf[idx].title),
                 "Possible Overfeeding Pattern");
        snprintf(buf[idx].message, sizeof(buf[idx].message),
                 "The bowl was still full at %.0f%% of recent feedings. Consider reducing the portion size or feeding frequency.",
                 ratio * 100.0f);
        return 1;
    }
    return 0;
}

// ── Helpers ───────────────────────────────────────────────────────
float AIEngine::_avgDispensedGrams() const {
    int total = min(_feedCount, HISTORY_SIZE);
    if (total == 0) return -1;

    float sum = 0;
    int count = 0;
    for (int i = 0; i < total; i++) {
        if (!_feedHistory[i].skipped) {
            sum += _feedHistory[i].dispensedGrams;
            count++;
        }
    }
    return (count > 0) ? (sum / count) : -1;
}

float AIEngine::_avgWaterDelta() const {
    int total = min(_waterCount, HISTORY_SIZE);
    if (total < 2) return 50;  // Default: assume normal consumption

    float sum = 0;
    for (int i = 0; i < total; i++) {
        float delta = _waterHistory[i].levelAfter - _waterHistory[i].levelBefore;
        sum += delta;
    }
    return sum / total;
}

int AIEngine::_consecutiveSkipped() const {
    int total = min(_feedCount, HISTORY_SIZE);
    int streak = 0;

    // Walk backwards through history
    for (int i = total - 1; i >= 0; i--) {
        int idx = (_feedCount - 1 - (total - 1 - i)) % HISTORY_SIZE;
        if (idx < 0) idx += HISTORY_SIZE;
        if (_feedHistory[idx].skipped || _feedHistory[idx].bowlLeftFull) {
            streak++;
        } else {
            break;
        }
    }
    return streak;
}

int AIEngine::daysUntilHopperRefill() const {
    if (_hopperEmptyEvents < 2) return -1;

    // Calculate average time between empty events (in milliseconds)
    unsigned long totalDuration = _lastHopperEmpty - _firstHopperEmpty;
    float avgIntervalMs = (float)totalDuration / (_hopperEmptyEvents - 1);

    // Predict next empty
    unsigned long timeSinceLast = millis() - _lastHopperEmpty;
    float msUntilNext = avgIntervalMs - timeSinceLast;

    if (msUntilNext <= 0) return 0;

    // Convert to days
    int days = (int)(msUntilNext / (1000UL * 60 * 60 * 24));
    return days;
}
