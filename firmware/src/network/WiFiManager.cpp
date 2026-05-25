#include "WiFiManager.h"

// ── AP constants ────────────────────────────────────────────────────────────
static const char* NVS_NAMESPACE = "wifi_cfg";
static const char* NVS_KEY_SSID  = "ssid";
static const char* NVS_KEY_PASS  = "pass";
static const char* AP_SSID       = "PetFeeder-Setup";   // SSID shown to phone

// ── Setup HTML (served in AP mode) ─────────────────────────────────────────
static const char SETUP_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Pet Feeder Setup</title>
  <style>
    *  { box-sizing: border-box; }
    body { font-family: sans-serif; max-width: 420px; margin: 40px auto; padding: 24px; }
    h2  { color: #2c7be5; margin-bottom: 4px; }
    p   { color: #555; margin-top: 0; }
    label { display: block; margin: 12px 0 4px; font-weight: 600; font-size: 14px; }
    input { width: 100%; padding: 10px 12px; border: 1px solid #ccc;
            border-radius: 8px; font-size: 15px; }
    input:focus { outline: none; border-color: #2c7be5; }
    button { margin-top: 20px; width: 100%; padding: 13px;
             background: #2c7be5; color: #fff; border: none;
             border-radius: 8px; font-size: 16px; cursor: pointer; }
    button:hover { background: #1a68d1; }
    .note { margin-top: 16px; font-size: 12px; color: #888; }
  </style>
</head>
<body>
  <h2>🐾 Pet Feeder WiFi Setup</h2>
  <p>Connect your feeder to your home WiFi network.</p>
  <form action="/save" method="POST">
    <label>WiFi Name (SSID)</label>
    <input type="text"     name="ssid" placeholder="e.g. MyHomeWiFi" required>
    <label>Password</label>
    <input type="password" name="pass" placeholder="Leave blank if open network">
    <button type="submit">Save &amp; Connect</button>
  </form>
  <p class="note">
    ✔ Hidden networks are supported — just type the exact SSID name.<br>
    ✔ The feeder will restart automatically after saving.
  </p>
</body>
</html>
)HTML";

static const char SAVED_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Saved</title>
  <style>
    body { font-family: sans-serif; max-width: 420px; margin: 60px auto;
           padding: 24px; text-align: center; }
    h2   { color: #28a745; }
    p    { color: #555; }
  </style>
</head>
<body>
  <h2>✅ Credentials Saved!</h2>
  <p>The feeder is restarting and will connect to your WiFi in a few seconds.</p>
  <p>You can close this page.</p>
</body>
</html>
)HTML";

// ── Constructor ──────────────────────────────────────────────────────────────
WiFiManager::WiFiManager()
    : _connected(false), _apMode(false),
      _lastAttempt(0), _retryCount(0),
      _server(nullptr), _dns(nullptr) {}

// ── Public ───────────────────────────────────────────────────────────────────
void WiFiManager::begin() {
    Serial.println("[WiFi] Starting...");

    String ssid, pass;
    if (_loadCredentials(ssid, pass)) {
        Serial.printf("[WiFi] Saved SSID: %s\n", ssid.c_str());
        _connectSTA(ssid, pass);
    } else {
        Serial.println("[WiFi] No credentials stored — entering AP setup mode.");
        _startAP();
    }
}

void WiFiManager::update() {
    // In AP mode: service DNS and web requests
    if (_apMode) {
        if (_dns)    _dns->processNextRequest();
        if (_server) _server->handleClient();
        return;
    }

    // In STA mode: monitor and reconnect
    if (WiFi.status() == WL_CONNECTED) {
        if (!_connected) {
            _connected  = true;
            _retryCount = 0;
            Serial.printf("[WiFi] Connected. IP: %s  RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
        }
        return;
    }

    _connected = false;

    unsigned long now = millis();
    if (now - _lastAttempt < WIFI_RECONNECT_DELAY_MS) return;
    _lastAttempt = now;

    if (_retryCount >= WIFI_MAX_RETRIES) {
        Serial.println("[WiFi] Max retries reached — switching to AP setup mode.");
        _startAP();
        return;
    }

    Serial.printf("[WiFi] Reconnecting... (%d/%d)\n", ++_retryCount, WIFI_MAX_RETRIES);
    WiFi.reconnect();
}

bool WiFiManager::isConnected() const { return _connected; }
bool WiFiManager::isAPMode()    const { return _apMode;    }

String WiFiManager::getIPAddress() const {
    return _apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

int WiFiManager::getRSSI() const {
    return _apMode ? 0 : WiFi.RSSI();
}

void WiFiManager::clearCredentials() {
    _prefs.begin(NVS_NAMESPACE, false);
    _prefs.clear();
    _prefs.end();
    Serial.println("[WiFi] Credentials cleared — restart to enter setup mode.");
}

// ── Private ──────────────────────────────────────────────────────────────────
bool WiFiManager::_loadCredentials(String& ssid, String& pass) {
    // 1. Try NVS (set via AP setup page)
    _prefs.begin(NVS_NAMESPACE, /*readOnly=*/true);
    ssid = _prefs.getString(NVS_KEY_SSID, "");
    pass = _prefs.getString(NVS_KEY_PASS, "");
    _prefs.end();

    if (ssid.length() > 0) {
        Serial.println("[WiFi] Using credentials from NVS (AP setup).");
        return true;
    }

    // 2. Fall back to compile-time credentials in config.h
    String fallbackSsid = String(WIFI_SSID);
    String fallbackPass = String(WIFI_PASSWORD);
    if (fallbackSsid.length() > 0 && fallbackSsid != "YOUR_WIFI_SSID") {
        Serial.println("[WiFi] NVS empty — using credentials from config.h.");
        ssid = fallbackSsid;
        pass = fallbackPass;
        return true;
    }

    return false;  // No credentials anywhere → go to AP mode
}

void WiFiManager::_saveCredentials(const String& ssid, const String& pass) {
    _prefs.begin(NVS_NAMESPACE, /*readOnly=*/false);
    _prefs.putString(NVS_KEY_SSID, ssid);
    _prefs.putString(NVS_KEY_PASS, pass);
    _prefs.end();
    Serial.printf("[WiFi] Credentials saved for: %s\n", ssid.c_str());
}

void WiFiManager::_connectSTA(const String& ssid, const String& pass) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false);   // clear any previous state
    delay(100);

    Serial.printf("[WiFi] Connecting to \"%s\" (pass len=%d)...\n",
                  ssid.c_str(), pass.length());

    // WiFi.begin() works for both visible AND hidden networks.
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long start = millis();
    wl_status_t status  = WL_IDLE_STATUS;

    while (millis() - start < 20000) {
        delay(500);
        status = WiFi.status();
        Serial.printf("[WiFi]  status=%d\n", status);
        if (status == WL_CONNECTED)         break;
        if (status == WL_CONNECT_FAILED)    break;
        if (status == WL_NO_SSID_AVAIL)     break;
    }

    if (status == WL_CONNECTED) {
        _connected = true;
        Serial.printf("[WiFi] ✓ Connected! IP: %s  RSSI: %d dBm\n",
            WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        const char* reason = "unknown";
        switch (status) {
            case WL_NO_SSID_AVAIL:  reason = "SSID not found (wrong name or 5GHz-only network)"; break;
            case WL_CONNECT_FAILED: reason = "connection refused (wrong password?)"; break;
            case WL_IDLE_STATUS:    reason = "timed out";          break;
            default: break;
        }
        Serial.printf("[WiFi] ✗ Failed: %s (status=%d)\n", reason, status);
        Serial.println("[WiFi]  → Will retry or fall back to AP mode.");
    }
}

void WiFiManager::_startAP() {
    // Tear down any existing STA connection
    WiFi.disconnect(true);
    delay(100);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);   // Open network (no password) — easy to join from phone

    IPAddress apIP = WiFi.softAPIP();   // typically 192.168.4.1

    Serial.println("[WiFi] ============================================");
    Serial.printf ("[WiFi]  AP Mode — SSID  : %s\n", AP_SSID);
    Serial.printf ("[WiFi]  Setup page       : http://%s\n", apIP.toString().c_str());
    Serial.println("[WiFi]  Connect phone to that WiFi, then open the URL.");
    Serial.println("[WiFi] ============================================");

    // Captive-portal DNS: redirect any hostname → our IP
    _dns = new DNSServer();
    _dns->start(53, "*", apIP);

    // Web server
    _server = new WebServer(80);

    _server->on("/",        HTTP_GET,  [this]() { _serveRoot();  });
    _server->on("/save",    HTTP_POST, [this]() { _handleSave(); });

    // Catch-all: redirect to setup page (makes captive portal work on Android/iOS)
    _server->onNotFound([this]() {
        _server->sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
        _server->send(302, "text/plain", "");
    });

    _server->begin();
    _apMode = true;
}

void WiFiManager::_serveRoot() {
    _server->send_P(200, "text/html", SETUP_HTML);
}

void WiFiManager::_handleSave() {
    String ssid = _server->arg("ssid");
    String pass = _server->arg("pass");

    if (ssid.isEmpty()) {
        _server->send(400, "text/plain", "SSID cannot be empty.");
        return;
    }

    _saveCredentials(ssid, pass);
    _server->send_P(200, "text/html", SAVED_HTML);

    Serial.println("[WiFi] Restarting in 3 seconds...");
    delay(3000);
    ESP.restart();
}
