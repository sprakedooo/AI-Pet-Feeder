#include "FirebaseManager.h"
#include <addons/TokenHelper.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

FirebaseManager::FirebaseManager()
    : _timeClient(_ntpUDP, NTP_SERVER, NTP_OFFSET_SEC, NTP_UPDATE_MS),
      _ready(false), _lastSync(0), _lastHeartbeat(0) {}

void FirebaseManager::begin() {
    // ── Step 1: Sync system clock BEFORE any SSL handshake ──────────────────
    // mbedTLS validates certificate dates — if the clock is wrong (epoch 0),
    // every SSL connection fails with "Failed to initialize the SSL layer".
    //
    // Strategy:
    //   a) Try SNTP (UDP 123) — fast when available.
    //   b) If that fails (UDP 123 blocked on campus/hotel networks), fall back
    //      to reading the "Date:" header from a plain HTTP request on port 80,
    //      which is almost never firewalled.

    Serial.print("[Firebase] Syncing clock via NTP");
    configTime(NTP_OFFSET_SEC, 0, NTP_SERVER, "time.google.com", "time.cloudflare.com");

    int attempts = 0;
    while (!_clockReady() && attempts < 20) {   // 10 s NTP window
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();

    if (_clockReady()) {
        Serial.printf("[Firebase] Clock ready via NTP: %ld\n", time(nullptr));
    } else {
        Serial.println("[Firebase] NTP blocked — trying HTTP Date fallback...");
        if (_syncTimeViaHTTP()) {
            Serial.printf("[Firebase] Clock ready via HTTP: %ld\n", time(nullptr));
        } else {
            Serial.println("[Firebase] WARNING: all time sync methods failed — SSL may not work.");
        }
    }

    // ── Step 2: Check free heap (SSL needs ~40 KB) ───────────────────────────
    Serial.printf("[Firebase] Free heap: %u bytes\n", ESP.getFreeHeap());

    // ── Step 3: Configure and start Firebase ────────────────────────────────
    _config.api_key               = FIREBASE_API_KEY;
    _config.database_url          = FIREBASE_DATABASE_URL;
    _config.token_status_callback = tokenStatusCallback;

    _auth.user.email    = FIREBASE_AUTH_EMAIL;
    _auth.user.password = FIREBASE_AUTH_PASSWORD;

    Firebase.begin(&_config, &_auth);
    Firebase.reconnectWiFi(true);

    _fbdo.setResponseSize(4096);
    _fbdo2.setResponseSize(2048);

    // Also start NTPClient for getCurrentHour() / getCurrentMinute()
    _timeClient.begin();
    _timeClient.update();

    Serial.println("[Firebase] Waiting for auth token...");

    unsigned long start = millis();
    while (!Firebase.ready() && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (Firebase.ready()) {
        _ready = true;
        Serial.println("[Firebase] Authenticated and ready.");

        // Clear any stale commands left over from a previous session.
        // Without this, old tare/reboot/feed flags in RTDB execute immediately
        // on boot, causing spurious feeds, taras, or reboots.
        FirebaseJson clearCmds;
        clearCmds.set("manualFeed",       false);
        clearCmds.set("manualWater",      false);
        clearCmds.set("emergencyStop",    false);
        clearCmds.set("tare",             false);
        clearCmds.set("reboot",           false);
        clearCmds.set("reloadSettings",   false);
        clearCmds.set("reloadSchedules",  false);
        clearCmds.set("resetWifi",        false);
        String cmdPath = _basePath() + "/commands";
        Firebase.RTDB.setJSON(&_fbdo, cmdPath.c_str(), &clearCmds);
        Serial.println("[Firebase] Stale commands cleared.");

        pushHeartbeat();
    } else {
        Serial.println("[Firebase] Auth timeout — will retry in update().");
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
                                 data.hopperStatus == HopperStatus::FOOD_LOW ? "low" : "empty");
    json.set("reservoirStatus",  data.reservoirHasWater ? "ok" : "empty");
    json.set("reservoirLevel",   data.reservoirLevelPercent);
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
    json.set("pulseCount",     rec.pulseCount);
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
    PendingCommands cmds = {false, false, false, false, false, false, false, false};
    if (!_ready) return cmds;

    String path = _basePath() + "/commands";
    if (!Firebase.RTDB.getJSON(&_fbdo2, path.c_str())) return cmds;

    // ── IMPORTANT: read ALL values BEFORE calling _clearCommand() ──────────
    // _clearCommand() writes to _fbdo2 via setBool(), which overwrites _fbdo2's
    // internal JSON buffer. Any json.get() call after the first clear reads
    // from a destroyed buffer and returns garbage (often boolValue=true),
    // causing spurious tare + reboot commands on every Feed Now / Water Now.
    FirebaseJson& json = _fbdo2.jsonObject();
    FirebaseJsonData result;

    json.get(result, "manualFeed");
    cmds.manualFeed = result.success && result.boolValue;

    json.get(result, "manualWater");
    cmds.manualWater = result.success && result.boolValue;

    json.get(result, "emergencyStop");
    cmds.emergencyStop = result.success && result.boolValue;

    json.get(result, "tare");
    cmds.tare = result.success && result.boolValue;

    json.get(result, "reboot");
    cmds.reboot = result.success && result.boolValue;

    json.get(result, "reloadSettings");
    cmds.reloadSettings = result.success && result.boolValue;

    json.get(result, "reloadSchedules");
    cmds.reloadSchedules = result.success && result.boolValue;

    json.get(result, "resetWifi");
    cmds.resetWifi = result.success && result.boolValue;

    // ── Now safe to clear — _fbdo2 is no longer needed for reading ──────────
    if (cmds.manualFeed)       _clearCommand("manualFeed");
    if (cmds.manualWater)      _clearCommand("manualWater");
    if (cmds.emergencyStop)    _clearCommand("emergencyStop");
    if (cmds.tare)             _clearCommand("tare");
    if (cmds.reboot)           _clearCommand("reboot");
    if (cmds.reloadSettings)   _clearCommand("reloadSettings");
    if (cmds.reloadSchedules)  _clearCommand("reloadSchedules");
    if (cmds.resetWifi)        _clearCommand("resetWifi");

    return cmds;
}

DeviceSettings FirebaseManager::getSettings() {
    DeviceSettings settings = {
        DEFAULT_TARGET_GRAMS,   // targetPortionGrams
        WATER_LOW_THRESHOLD,    // waterLowThreshold
        PET_DETECTION_DISTANCE_CM, // detectionDistanceCm
        true,                   // autoMode
        true,                   // notificationsEnabled
        // Calibration defaults — overwritten below if present in RTDB
        WATER_EMPTY_ADC,
        WATER_FULL_ADC,
        RESERVOIR_EMPTY_ADC,
        RESERVOIR_FULL_ADC,
        LOAD_CELL_CALIBRATION_FACTOR
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

    // ── Sensor calibration (written by the debug menu in the Flutter app) ──
    json.get(result, "calibration/waterEmptyADC");
    if (result.success && result.intValue > 0) settings.waterEmptyADC = result.intValue;

    json.get(result, "calibration/waterFullADC");
    if (result.success && result.intValue > 0) settings.waterFullADC = result.intValue;

    json.get(result, "calibration/reservoirEmptyADC");
    if (result.success && result.intValue > 0) settings.reservoirEmptyADC = result.intValue;

    json.get(result, "calibration/reservoirFullADC");
    if (result.success && result.intValue > 0) settings.reservoirFullADC = result.intValue;

    json.get(result, "calibration/loadCellFactor");
    if (result.success && result.floatValue != 0.0f) settings.calibrationFactor = result.floatValue;

    return settings;
}

unsigned long FirebaseManager::getEpochTime() {
    // Prefer NTPClient when it has synced successfully; otherwise use the
    // system clock set by configTime() or our HTTP fallback via settimeofday().
    if (_timeClient.isTimeSet()) return _timeClient.getEpochTime();
    time_t now = time(nullptr);
    return (now > 0) ? (unsigned long)now : 0;
}

int FirebaseManager::getCurrentHour() {
    if (_timeClient.isTimeSet()) return _timeClient.getHours();
    time_t now = time(nullptr);
    struct tm* t = gmtime(&now);
    return t ? t->tm_hour : 0;
}

int FirebaseManager::getCurrentMinute() {
    if (_timeClient.isTimeSet()) return _timeClient.getMinutes();
    time_t now = time(nullptr);
    struct tm* t = gmtime(&now);
    return t ? t->tm_min : 0;
}

int FirebaseManager::getCurrentDayOfWeek() {
    // NTPClient::getDay() returns 0=Sunday, 1=Monday, ..., 6=Saturday
    if (_timeClient.isTimeSet()) return _timeClient.getDay();
    time_t now = time(nullptr);
    struct tm* t = gmtime(&now);
    return t ? t->tm_wday : 0; // tm_wday: 0=Sunday, 6=Saturday
}

// Reads the schedule list from RTDB (devices/<id>/settings_cache/schedules).
// Each item is fetched individually at path /schedules/0, /schedules/1, ...
// This avoids array-vs-object ambiguity in the Firebase Arduino JSON parser.
// Returns the number of valid schedule entries filled into `out`.
int FirebaseManager::getSchedules(FeedSchedule* out, int maxCount) {
    if (!_ready || maxCount <= 0) return 0;

    String basePath = _basePath() + "/settings_cache/schedules";
    int count = 0;

    for (int i = 0; i < maxCount; i++) {
        // Read each schedule item by its integer index path (/0, /1, ...)
        String itemPath = basePath + "/" + String(i);

        if (!Firebase.RTDB.getJSON(&_fbdo2, itemPath.c_str())) {
            // No item at this index — end of list
            break;
        }

        FirebaseJson&    json = _fbdo2.jsonObject();
        FirebaseJsonData d;

        json.get(d, "enabled");
        if (!d.success) break; // malformed item — stop

        out[i].enabled      = d.boolValue;
        out[i].triggered    = false;

        json.get(d, "hour");
        out[i].hour = d.success ? d.intValue : 8;

        json.get(d, "minute");
        out[i].minute = d.success ? d.intValue : 0;

        json.get(d, "portionGrams");
        out[i].portionGrams = d.success ? d.floatValue : DEFAULT_TARGET_GRAMS;

        // daysMask: bitmask integer — bit0=Sun, bit1=Mon, ..., bit6=Sat.
        // 0x7F = every day (written by Flutter app instead of a days array).
        json.get(d, "daysMask");
        out[i].days = (d.success && d.intValue > 0) ? (uint8_t)d.intValue : 0x7F;

        Serial.printf("[Firebase] Schedule[%d]: %02d:%02d %.0fg en=%d days=0x%02X\n",
                      i, out[i].hour, out[i].minute, out[i].portionGrams,
                      out[i].enabled, out[i].days);
        count++;
    }

    Serial.printf("[Firebase] Loaded %d schedule(s).\n", count);
    return count;
}

bool FirebaseManager::isReady() const {
    return _ready;
}

bool FirebaseManager::_clockReady() {
    return time(nullptr) > 1700000000L;  // any date after Nov 2023 is plausible
}

// ── Shared Date-header parser ────────────────────────────────────────────────
// Parses "Sat, 24 May 2025 11:44:00 GMT" → UTC epoch.
// Does NOT use mktime() because configTime(UTC+8,...) makes mktime() treat
// the input as local time, giving a result 8 h off.
static time_t _parseHttpDate(const String& dateStr) {
    if (dateStr.length() < 25) return 0;
    const char* months[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    struct tm t = {};
    t.tm_mday = dateStr.substring(5, 7).toInt();
    t.tm_year = dateStr.substring(12, 16).toInt() - 1900;
    t.tm_hour = dateStr.substring(17, 19).toInt();
    t.tm_min  = dateStr.substring(20, 22).toInt();
    t.tm_sec  = dateStr.substring(23, 25).toInt();
    String mon = dateStr.substring(8, 11);
    for (int i = 0; i < 12; i++)
        if (mon == months[i]) { t.tm_mon = i; break; }

    static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int yr = t.tm_year + 1900;
    long days = (long)(yr - 1970) * 365;
    for (int y = 1970; y < yr; y++)
        if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) days++;
    for (int m = 0; m < t.tm_mon; m++) {
        days += dim[m];
        if (m == 1 && ((yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0)) days++;
    }
    days += t.tm_mday - 1;
    return days * 86400L + (long)t.tm_hour * 3600 + (long)t.tm_min * 60 + t.tm_sec;
}

// ── Reads the Date header from an already-connected client ──────────────────
template <typename T>
static time_t _readDateHeader(T& client) {
    unsigned long deadline = millis() + 5000;
    while (client.connected() && millis() < deadline) {
        if (!client.available()) { delay(10); continue; }
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) break;
        if (line.startsWith("Date: ") || line.startsWith("date: "))
            return _parseHttpDate(line.substring(6));
    }
    return 0;
}

bool FirebaseManager::_syncTimeViaHTTP() {
    // ── Pass 1: plain HTTP port 80 ───────────────────────────────────────────
    {
        WiFiClient client;
        const char* hosts80[] = { "time.cloudflare.com", "www.google.com", nullptr };
        for (int i = 0; hosts80[i]; i++) {
            if (client.connect(hosts80[i], 80)) {
                client.printf("HEAD / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                              hosts80[i]);
                time_t epoch = _readDateHeader(client);
                client.stop();
                if (epoch > 1700000000L) {
                    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
                    settimeofday(&tv, nullptr);
                    Serial.printf("[Time] HTTP port 80 sync OK: %ld\n", epoch);
                    return true;
                }
            }
        }
        Serial.println("[Time] Port 80 blocked — trying HTTPS port 443...");
    }

    // ── Pass 2: HTTPS port 443 with cert validation DISABLED ────────────────
    // We use setInsecure() only for this one time-sync request.
    // The Date header carries no sensitive data — we just need the timestamp.
    // All Firebase connections afterwards use normal full TLS validation.
    {
        WiFiClientSecure client;
        client.setInsecure();   // skip cert check — clock isn't set yet anyway
        client.setTimeout(8);

        const char* hosts443[] = { "time.cloudflare.com", "www.google.com", nullptr };
        for (int i = 0; hosts443[i]; i++) {
            Serial.printf("[Time] Connecting to %s:443...\n", hosts443[i]);
            if (client.connect(hosts443[i], 443)) {
                client.printf("HEAD / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                              hosts443[i]);
                time_t epoch = _readDateHeader(client);
                client.stop();
                if (epoch > 1700000000L) {
                    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
                    settimeofday(&tv, nullptr);
                    Serial.printf("[Time] HTTPS port 443 sync OK: %ld\n", epoch);
                    return true;
                }
            } else {
                Serial.printf("[Time] Could not connect to %s:443\n", hosts443[i]);
            }
        }
    }

    return false;
}

String FirebaseManager::_basePath() {
    return String("/devices/") + DEVICE_ID;
}

void FirebaseManager::_clearCommand(const char* command) {
    // Use _fbdo (not _fbdo2) so we never corrupt an in-progress _fbdo2 read.
    String path = _basePath() + "/commands/" + command;
    Firebase.RTDB.setBool(&_fbdo, path.c_str(), false);
}
