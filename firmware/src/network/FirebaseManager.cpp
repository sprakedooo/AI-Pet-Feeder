#include "FirebaseManager.h"

FirebaseManager::FirebaseManager()
    : _timeClient(_ntpUDP, NTP_SERVER, NTP_OFFSET_SEC, NTP_UPDATE_MS),
      _ready(false), _lastSync(0), _lastHeartbeat(0) {}

void FirebaseManager::begin() {
    _config.api_key           = FIREBASE_API_KEY;
    _config.database_url      = FIREBASE_DATABASE_URL;
    _config.token_status_callback = tokenStatusCallback;

    _auth.user.email    = FIREBASE_AUTH_EMAIL;
    _auth.user.password = FIREBASE_AUTH_PASSWORD;

    Firebase.begin(&_config, &_auth);
    Firebase.reconnectWiFi(true);

    _fbdo.setResponseSize(4096);
    _fbdo2.setResponseSize(2048);

    // Start NTP
    _timeClient.begin();
    _timeClient.update();

    Serial.println("[Firebase] Initialized. Waiting for auth token...");

    // Wait up to 10s for token
    unsigned long start = millis();
    while (!Firebase.ready() && millis() - start < 10000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (Firebase.ready()) {
        _ready = true;
        Serial.println("[Firebase] Authenticated and ready.");
        pushHeartbeat();
    } else {
        Serial.println("[Firebase] Auth timeout — will retry in loop.");
    }
}

void FirebaseManager::update() {
    if (!Firebase.ready()) {
        _ready = false;
        return;
    }
    _ready = true;
    _timeClient.update();
}

void FirebaseManager::pushSensorData(const SensorData& data) {
    if (!_ready) return;

    FirebaseJson json;
    json.set("bowlWeight",       data.bowlWeightGrams);
    json.set("bowlWeightPercent", data.bowlWeightPercent);
    json.set("waterLevel",       data.waterLevelPercent);
    json.set("hopperStatus",     data.hopperStatus == HopperStatus::OK ? "ok" :
                                 data.hopperStatus == HopperStatus::LOW ? "low" : "empty");
    json.set("reservoirStatus",  data.reservoirHasWater ? "ok" : "empty");
    json.set("petDetected",      data.petDetected);
    json.set("petDistance",      data.petDistanceCm);
    json.set("lastUpdated",      (int)getEpochTime());

    String path = _basePath() + "/sensors";
    if (!Firebase.RTDB.setJSON(&_fbdo, path.c_str(), &json)) {
        Serial.printf("[Firebase] Sensor push failed: %s\n", _fbdo.errorReason().c_str());
    }
}

void FirebaseManager::pushState(const char* state, bool isDispensing, bool isPumping) {
    if (!_ready) return;

    FirebaseJson json;
    json.set("currentState",  state);
    json.set("isDispensing",  isDispensing);
    json.set("isPumping",     isPumping);
    json.set("lastUpdated",   (int)getEpochTime());

    String path = _basePath() + "/state";
    Firebase.RTDB.setJSON(&_fbdo, path.c_str(), &json);
}

void FirebaseManager::pushFeedingLog(const FeedingRecord& rec) {
    if (!_ready) return;

    const char* resultStr = "unknown";
    switch (rec.result) {
        case FeedResult::SUCCESS:                 resultStr = "success"; break;
        case FeedResult::SKIPPED_BOWL_FULL:       resultStr = "skipped_bowl_full"; break;
        case FeedResult::SKIPPED_HOPPER_EMPTY:    resultStr = "skipped_hopper_empty"; break;
        case FeedResult::FAILED_TIMEOUT:          resultStr = "failed_timeout"; break;
        case FeedResult::FAILED_JAM:              resultStr = "failed_jam"; break;
        case FeedResult::FAILED_NO_WEIGHT_CHANGE: resultStr = "failed_no_change"; break;
    }

    FirebaseJson json;
    json.set("deviceId",       DEVICE_ID);
    json.set("status",         resultStr);
    json.set("targetGrams",    rec.targetGrams);
    json.set("dispensedGrams", rec.dispensedGrams);
    json.set("bowlBefore",     rec.bowlBefore);
    json.set("bowlAfter",      rec.bowlAfter);
    json.set("retryCount",     rec.retryCount);
    json.set("jamDetected",    rec.jamDetected);
    json.set("durationMs",     (int)rec.durationMs);
    json.set("timestamp",      (int)getEpochTime());

    // Push to RTDB log node (latest only)
    String rtdbPath = _basePath() + "/lastFeedingLog";
    Firebase.RTDB.setJSON(&_fbdo, rtdbPath.c_str(), &json);
}

void FirebaseManager::pushWaterLog(const WaterRecord& rec) {
    if (!_ready) return;

    const char* resultStr = "unknown";
    switch (rec.result) {
        case WaterResult::SUCCESS:                 resultStr = "success"; break;
        case WaterResult::SKIPPED_BOWL_FULL:       resultStr = "skipped_full"; break;
        case WaterResult::SKIPPED_RESERVOIR_EMPTY: resultStr = "skipped_reservoir_empty"; break;
        case WaterResult::FAILED_TIMEOUT:          resultStr = "failed_timeout"; break;
        case WaterResult::FAILED_DRY_RUN:          resultStr = "failed_dry_run"; break;
    }

    FirebaseJson json;
    json.set("deviceId",       DEVICE_ID);
    json.set("status",         resultStr);
    json.set("levelBefore",    rec.levelBefore);
    json.set("levelAfter",     rec.levelAfter);
    json.set("pumpDurationMs", (int)rec.pumpDurationMs);
    json.set("timestamp",      (int)getEpochTime());

    String rtdbPath = _basePath() + "/lastWaterLog";
    Firebase.RTDB.setJSON(&_fbdo, rtdbPath.c_str(), &json);
}

void FirebaseManager::pushHeartbeat() {
    if (!Firebase.ready()) return;

    FirebaseJson json;
    json.set("online",           true);
    json.set("lastSeen",         (int)getEpochTime());
    json.set("ipAddress",        WiFi.localIP().toString());
    json.set("firmwareVersion",  FIRMWARE_VERSION);
    json.set("wifiRSSI",         WiFi.RSSI());

    String path = _basePath() + "/status";
    Firebase.RTDB.setJSON(&_fbdo, path.c_str(), &json);
    _lastHeartbeat = millis();
}

void FirebaseManager::sendNotification(const char* type, const char* title, const char* body) {
    if (!_ready) return;

    FirebaseJson json;
    json.set("type",      type);
    json.set("title",     title);
    json.set("body",      body);
    json.set("isRead",    false);
    json.set("timestamp", (int)getEpochTime());
    json.set("deviceId",  DEVICE_ID);

    // Push to a notifications queue node — app reads and moves to Firestore
    String path = _basePath() + "/pendingNotification";
    Firebase.RTDB.setJSON(&_fbdo2, path.c_str(), &json);
    Serial.printf("[Firebase] Notification: [%s] %s\n", type, title);
}

PendingCommands FirebaseManager::checkCommands() {
    PendingCommands cmds = {false, false, false, false, false};
    if (!_ready) return cmds;

    String path = _basePath() + "/commands";
    if (!Firebase.RTDB.getJSON(&_fbdo2, path.c_str())) return cmds;

    FirebaseJson& json = _fbdo2.jsonObject();
    FirebaseJsonData result;

    json.get(result, "manualFeed");
    if (result.success && result.boolValue) {
        cmds.manualFeed = true;
        _clearCommand("manualFeed");
    }

    json.get(result, "manualWater");
    if (result.success && result.boolValue) {
        cmds.manualWater = true;
        _clearCommand("manualWater");
    }

    json.get(result, "emergencyStop");
    if (result.success && result.boolValue) {
        cmds.emergencyStop = true;
        _clearCommand("emergencyStop");
    }

    json.get(result, "tare");
    if (result.success && result.boolValue) {
        cmds.tare = true;
        _clearCommand("tare");
    }

    json.get(result, "reboot");
    if (result.success && result.boolValue) {
        cmds.reboot = true;
        _clearCommand("reboot");
    }

    return cmds;
}

DeviceSettings FirebaseManager::getSettings() {
    DeviceSettings settings = {
        DEFAULT_TARGET_GRAMS,
        WATER_LOW_THRESHOLD,
        PET_DETECTION_DISTANCE_CM,
        true,
        true
    };

    if (!_ready) return settings;

    String path = _basePath() + "/settings_cache";
    if (!Firebase.RTDB.getJSON(&_fbdo2, path.c_str())) return settings;

    FirebaseJson& json = _fbdo2.jsonObject();
    FirebaseJsonData result;

    json.get(result, "targetPortionGrams");
    if (result.success) settings.targetPortionGrams = result.floatValue;

    json.get(result, "waterLowThreshold");
    if (result.success) settings.waterLowThreshold = result.intValue;

    json.get(result, "detectionDistance");
    if (result.success) settings.detectionDistanceCm = result.intValue;

    json.get(result, "autoMode");
    if (result.success) settings.autoMode = result.boolValue;

    return settings;
}

unsigned long FirebaseManager::getEpochTime() {
    return _timeClient.getEpochTime();
}

int FirebaseManager::getCurrentHour() {
    return _timeClient.getHours();
}

int FirebaseManager::getCurrentMinute() {
    return _timeClient.getMinutes();
}

bool FirebaseManager::isReady() const {
    return _ready;
}

String FirebaseManager::_basePath() {
    return String("/devices/") + DEVICE_ID;
}

void FirebaseManager::_clearCommand(const char* command) {
    String path = _basePath() + "/commands/" + command;
    Firebase.RTDB.setBool(&_fbdo2, path.c_str(), false);
}
