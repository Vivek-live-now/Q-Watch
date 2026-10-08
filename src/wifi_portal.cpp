#include "ui_core.h"
#include "wifi_portal.h"
#include "config.h"
#include "weather.h"
#include "clock.h"
#include "settings_data.h"
#include "qlink.h"
#include "air_mouse.h"
#include <ESPmDNS.h>
#include <esp_wifi.h>

WifiPortal wifiPortal;

const byte DNS_PORT = 53;

WifiPortal::WifiPortal() : server(80), state(WifiState::OFF), connect_start_time(0), last_reconnect_attempt(0), scan_in_progress(false), scanned_count(0) {}

void WifiPortal::begin() {
    configManager.load();

    if (!settingsManager.get().wifi_enabled) {
        disableWifi();
        return;
    }

    enableWifi();
}

void WifiPortal::configurePowerSave() {
#ifdef ARDUINO
    bool ble_in_use = (settingsManager.get().ble_enabled || qlink.isBleActive() || airMouse.isEnabled());
    if (ble_in_use) {
        // When BLE is active, ESP-IDF coexistence strictly requires Wi-Fi modem sleep.
        // Calling esp_wifi_set_ps(WIFI_PS_NONE) causes coex aborts and massive peak current spikes!
        WiFi.setSleep(true);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    } else {
        WiFi.setSleep(false);
        esp_wifi_set_ps(WIFI_PS_NONE);
    }
#endif
}

void WifiPortal::applyTxPower() {
    int idx = settingsManager.get().wifi_tx_power_idx;
    static const int8_t esp_powers[] = {78, 60, 44, 28, 8};
    static const wifi_power_t wifi_powers[] = {
        WIFI_POWER_19_5dBm,
        WIFI_POWER_15dBm,
        WIFI_POWER_11dBm,
        WIFI_POWER_7dBm,
        WIFI_POWER_2dBm
    };
    if (idx < 0 || idx >= 5) idx = 0;

#ifdef ARDUINO
    bool ble_in_use = (settingsManager.get().ble_enabled || qlink.isBleActive() || airMouse.isEnabled());
    if (ble_in_use && idx == 0) {
        // Clamp maximum Wi-Fi TX power from 19.5 dBm down to 15 dBm during concurrent BLE operation
        // to prevent peak current draw from exceeding the SuperMini onboard LDO regulator
        WiFi.setTxPower(wifi_powers[1]);
        esp_wifi_set_max_tx_power(esp_powers[1]);
    } else {
        WiFi.setTxPower(wifi_powers[idx]);
        esp_wifi_set_max_tx_power(esp_powers[idx]);
    }
#else
    WiFi.setTxPower(wifi_powers[idx]);
#endif
}

void WifiPortal::enableWifi() {
    AppConfig& cfg = configManager.get();
    WiFi.hostname("Q-Watch");

    if (cfg.wifi_ssid.length() > 0) {
        Serial.print("Connecting to Wi-Fi: ");
        Serial.println(cfg.wifi_ssid);
        WiFi.mode(WIFI_STA);

        // Configure Wi-Fi TX power and modem sleep / coexistence for ESP32-S3 SuperMini
        configurePowerSave();
        applyTxPower();
        esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

        WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_password.c_str());
        state = WifiState::CONNECTING;
        connect_start_time = millis();
    } else {
        Serial.println("Wi-Fi enabled but no credentials found.");
        WiFi.mode(WIFI_STA);
        configurePowerSave();
        applyTxPower();
        state = WifiState::NO_CREDS;
    }
}

void WifiPortal::disableWifi() {
    Serial.println("Disabling Wi-Fi radio...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    state = WifiState::OFF;
}

void WifiPortal::startScan() {
    if (!settingsManager.get().wifi_enabled) {
        settingsManager.get().wifi_enabled = true;
        settingsManager.save();
    }

    WiFi.mode(WIFI_STA);
    configurePowerSave();
    applyTxPower();

    WiFi.scanDelete();
    int res = WiFi.scanNetworks(true);
    if (res != WIFI_SCAN_FAILED) {
        scan_in_progress = true;
        scanned_count = 0;
    }
}

void WifiPortal::connectToNetwork(const String& ssid, const String& password) {
    AppConfig cfg = configManager.get();
    cfg.wifi_ssid = ssid;
    cfg.wifi_password = password;
    configManager.set(cfg);
    configManager.save();

    settingsManager.get().wifi_enabled = true;
    settingsManager.save();

    enableWifi();
}

void WifiPortal::startPortal() {
    Serial.println("Starting Captive Portal...");
    state = WifiState::PORTAL;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Q-Watch-Setup");

    configurePowerSave();
    applyTxPower();

    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    setupRoutes();
    server.begin();
    Serial.println("Portal running.");
}

void WifiPortal::stopPortal() {
    if (state != WifiState::PORTAL) return;
    Serial.println("Stopping Captive Portal / Hotspot...");
    dnsServer.stop();
    server.stop();
    WiFi.softAPdisconnect(true);
    enableWifi();
}

void WifiPortal::setupRoutes() {
    server.on("/", HTTP_GET, std::bind(&WifiPortal::handleRoot, this));
    server.on("/save", HTTP_POST, std::bind(&WifiPortal::handleSave, this));
    server.on("/scan_trigger", HTTP_GET, std::bind(&WifiPortal::handleScanTrigger, this));
    server.on("/scan_results", HTTP_GET, std::bind(&WifiPortal::handleScanResults, this));
    server.on("/status_json", HTTP_GET, std::bind(&WifiPortal::handleStatusJson, this));
    server.on("/weather_force", HTTP_GET, std::bind(&WifiPortal::handleWeatherForce, this));
    server.on("/fm", HTTP_GET, std::bind(&WifiPortal::handleFileManagerGui, this));
    server.on("/file_list", HTTP_GET, std::bind(&WifiPortal::handleFileList, this));
    server.on("/file_download", HTTP_GET, std::bind(&WifiPortal::handleFileDownload, this));
    server.on("/file_delete", HTTP_POST, std::bind(&WifiPortal::handleFileDelete, this));
    server.on("/file_mkdir", HTTP_POST, std::bind(&WifiPortal::handleFileMkdir, this));
    server.on("/file_rename", HTTP_POST, std::bind(&WifiPortal::handleFileRename, this));
    server.on("/file_copy", HTTP_POST, std::bind(&WifiPortal::handleFileCopy, this));
    server.on("/file_upload", HTTP_POST, [this]() {
        server.send(200, "text/plain", "Upload Successful");
    }, std::bind(&WifiPortal::handleFileUpload, this));

    // Register Q-Link Protocol API Routes
    qlink.registerHttpRoutes(server);

    server.onNotFound([this]() {
        if (state == WifiState::PORTAL) {
            server.sendHeader("Location", "http://192.168.4.1/", true);
            server.send(302, "text/plain", "");
        } else {
            server.send(404, "text/plain", "Not Found");
        }
    });
}

void WifiPortal::loop() {
    if (scan_in_progress) {
        int n = WiFi.scanComplete();
        if (n >= 0) {
            scan_in_progress = false;
            scanned_count = min(n, 16);
            for (int i = 0; i < scanned_count; ++i) {
                scanned_networks[i].ssid = WiFi.SSID(i);
                scanned_networks[i].rssi = WiFi.RSSI(i);
                scanned_networks[i].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            }
            WiFi.scanDelete();
        } else if (n == WIFI_SCAN_FAILED) {
            scan_in_progress = false;
            scanned_count = 0;
        }
    }

    if (!settingsManager.get().wifi_enabled) {
        if (state != WifiState::OFF) {
            disableWifi();
        }
        return;
    }

    if (state == WifiState::CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Wi-Fi connected.");
            Serial.println(WiFi.localIP());

            if (MDNS.begin("q-watch")) {
                Serial.println("mDNS responder started at q-watch.local");
            }

            setupRoutes();
            server.begin();
            state = WifiState::CONNECTED;

            // Trigger auto NTP sync on Wi-Fi connection if enabled
            if (settingsManager.get().auto_sync) {
                qclock.syncNtp();
            }
        } else if (millis() - connect_start_time > 15000) {
            Serial.println("Wi-Fi connection attempt failed/timed out.");
            state = WifiState::FAILED;
        }
    } else if (state == WifiState::CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Wi-Fi disconnected.");
            state = WifiState::DISCONNECTED;
            last_reconnect_attempt = millis();
        } else {
            server.handleClient();

            // Wi-Fi Auto-off Mode 2: When Idle (e.g. no user activity for 2 minutes)
            if (settingsManager.get().wifi_auto_off_idx == 2) {
                if (millis() - ui.getLastActivityTime() > 120000) {
                    Serial.println("Wi-Fi auto-off due to system idle...");
                    disableWifi();
                }
            }
        }
    } else if (state == WifiState::DISCONNECTED || state == WifiState::FAILED) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Wi-Fi reconnected.");
            state = WifiState::CONNECTED;
            if (settingsManager.get().auto_sync) {
                qclock.syncNtp();
            }
        } else if (last_reconnect_attempt > 0 && (millis() - last_reconnect_attempt > 30000)) {
            AppConfig& cfg = configManager.get();
            if (cfg.wifi_ssid.length() > 0) {
                Serial.println("Attempting Wi-Fi reconnect...");
                WiFi.reconnect();
                state = WifiState::CONNECTING;
                connect_start_time = millis();
                last_reconnect_attempt = millis();
            }
        }
    } else if (state == WifiState::PORTAL) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
}

WifiState WifiPortal::getState() {
    return state;
}

const char* WifiPortal::getDetailedStatusStr() {
    switch (state) {
        case WifiState::OFF: return "OFF";
        case WifiState::NO_CREDS: return "NO CREDS";
        case WifiState::CONNECTING: return "CONNECTING";
        case WifiState::CONNECTED: return "CONNECTED";
        case WifiState::FAILED: return "FAILED";
        case WifiState::DISCONNECTED: return "DISCONNECTED";
        case WifiState::PORTAL: return "PORTAL";
    }
    return "UNKNOWN";
}

String WifiPortal::getSSID() {
    if (state == WifiState::PORTAL) {
        return "Q-Watch-Setup";
    }
    if (state == WifiState::CONNECTED) {
        return WiFi.SSID();
    }
    AppConfig& cfg = configManager.get();
    if (cfg.wifi_ssid.length() > 0) {
        return cfg.wifi_ssid;
    }
    return "-";
}

String WifiPortal::getIP() {
    if (state == WifiState::CONNECTED) {
        return WiFi.localIP().toString();
    }
    if (state == WifiState::PORTAL) {
        return WiFi.softAPIP().toString();
    }
    return "-";
}

void WifiPortal::handleRoot() {
    server.send(200, "text/html", getHtml());
}

void WifiPortal::handleStatusJson() {
    String json;
    json.reserve(256);
    json = "{";
    json += "\"wifi\": \"" + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\",";
    json += "\"ip\": \"" + WiFi.localIP().toString() + "\",";
    json += "\"rssi\": " + String(WiFi.RSSI()) + ",";
    json += "\"time_sync\": \"" + String(qclock.isTimeSet() ? "Synced" : "Not Synced") + "\",";

    uint32_t lut = weather.getLastUpdateTime();
    String w_sync = weather.isUpdateInProgress() ? "Syncing..." : (lut > 0 ? "Synced" : "Pending");
    String w_last = lut > 0 ? String((millis() - lut) / 1000) + " s ago" : "Never";

    json += "\"w_sync\": \"" + w_sync + "\",";
    json += "\"w_last\": \"" + w_last + "\",";
    json += "\"uptime\": " + String(millis() / 1000) + "}";
    server.send(200, "application/json", json);
}

void WifiPortal::handleWeatherForce() {
    weather.forceUpdate();
    server.send(200, "text/plain", "OK");
}

void WifiPortal::handleScanTrigger() {
    if (!scan_in_progress) {
        startScan();
    }
    server.send(200, "text/plain", "STARTED");
}

void WifiPortal::handleScanResults() {
    if (scan_in_progress) {
        server.send(200, "application/json", "{\"status\":\"running\"}");
    } else {
        String json;
        json.reserve(scanned_count * 64 + 64);
        json = "{\"status\":\"complete\",\"networks\":[";
        for (int i = 0; i < scanned_count; ++i) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + scanned_networks[i].ssid + "\",\"rssi\":" + String(scanned_networks[i].rssi) + ",\"enc\":" + String(scanned_networks[i].encrypted ? 1 : 0) + "}";
        }
        json += "]}";
        server.send(200, "application/json", json);
    }
}

void WifiPortal::handleSave() {
    AppConfig cfg = configManager.get();

    if (server.hasArg("ssid")) cfg.wifi_ssid = server.arg("ssid");
    if (server.hasArg("pass") && server.arg("pass").length() > 0) {
        cfg.wifi_password = server.arg("pass");
    }
    if (server.hasArg("tz")) cfg.timezone = server.arg("tz");
    if (server.hasArg("lat")) cfg.latitude = server.arg("lat").toFloat();
    if (server.hasArg("lon")) cfg.longitude = server.arg("lon").toFloat();

    if (server.hasArg("owm_key") && server.arg("owm_key").length() > 0) {
        cfg.owm_api_key = server.arg("owm_key");
    }

    if (server.hasArg("owm_loc")) cfg.owm_location = server.arg("owm_loc");
    if (server.hasArg("owm_unt")) cfg.owm_units = server.arg("owm_unt");
    if (server.hasArg("w_int")) cfg.weather_interval_ms = server.arg("w_int").toInt() * 60 * 1000;

    configManager.set(cfg);
    configManager.save();

    server.send(200, "text/html", "<html><body><h1>Settings Saved! Rebooting...</h1></body></html>");
    delay(1000);
    ESP.restart();
}

static const char DASHBOARD_HTML_TEMPLATE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Q-Watch Dashboard</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background: #121212; color: #f0f0f0; margin: 0; padding: 0; }
        .header { background: #1e1e1e; padding: 20px; text-align: center; border-bottom: 2px solid #333; }
        .container { padding: 20px; max-width: 600px; margin: auto; }
        .card { background: #1e1e1e; border-radius: 8px; padding: 15px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        h2 { margin-top: 0; border-bottom: 1px solid #333; padding-bottom: 10px; font-size: 1.2em; color: #00bcd4; }
        label { display: block; margin-top: 10px; font-size: 0.9em; color: #aaa; }
        input, select { width: 100%; box-sizing: border-box; padding: 10px; margin-top: 5px; background: #2c2c2c; color: #fff; border: 1px solid #444; border-radius: 4px; }
        button { background: #00bcd4; color: #fff; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; font-weight: bold; width: 100%; margin-top: 15px; }
        button:hover { background: #0097a7; }
        .secondary-btn { background: #444; margin-top: 5px; }
        .secondary-btn:hover { background: #555; }
        .status-row { display: flex; justify-content: space-between; padding: 5px 0; border-bottom: 1px solid #333; font-size: 0.9em; }
        .net-item { padding: 10px; background: #2c2c2c; margin-bottom: 5px; border-radius: 4px; cursor: pointer; display: flex; justify-content: space-between; }
        .net-item:hover { background: #3c3c3c; }
        #scanResults { max-height: 200px; overflow-y: auto; margin-top: 10px; display: none; }
    </style>
    <script>
        function fetchStatus() {
            fetch('/status_json').then(r=>r.json()).then(data => {
                document.getElementById('st_wifi').innerText = data.wifi;
                document.getElementById('st_ip').innerText = data.ip;
                document.getElementById('st_rssi').innerText = data.rssi + " dBm";
                document.getElementById('st_time').innerText = data.time_sync;
                document.getElementById('st_wsync').innerText = data.w_sync;
                document.getElementById('st_wlast').innerText = data.w_last;
                document.getElementById('st_up').innerText = data.uptime + " s";
            }).catch(e => console.error(e));
        }
        function pollScanResults() {
            fetch('/scan_results').then(r=>r.json()).then(data => {
                let res = document.getElementById('scanResults');
                if (data.status === 'running') {
                    setTimeout(pollScanResults, 1000);
                } else if (data.status === 'complete') {
                    res.innerHTML = "";
                    data.networks.forEach(net => {
                        let d = document.createElement('div');
                        d.className = 'net-item';
                        d.innerHTML = `<span>${net.ssid} ${net.enc ? 'SECURE' : 'OPEN'}</span><span>${net.rssi} dBm</span>`;
                        d.onclick = () => { document.getElementById('ssid').value = net.ssid; res.style.display='none'; };
                        res.appendChild(d);
                    });
                } else {
                    res.innerHTML = "Scan failed.";
                }
            }).catch(e => { document.getElementById('scanResults').innerHTML = "Scan error."; });
        }
        function scanWifi() {
            let res = document.getElementById('scanResults');
            res.style.display = 'block';
            res.innerHTML = "Triggering Async Scan...";
            fetch('/scan_trigger').then(() => {
                res.innerHTML = "Scanning in background...";
                setTimeout(pollScanResults, 1000);
            });
        }
        function forceWeather() {
            fetch('/weather_force').then(() => alert('Weather update triggered.'));
        }
        setInterval(fetchStatus, 5000);
        window.onload = fetchStatus;
    </script>
</head>
<body>
    <div class="header">
        <h1>Q-Watch Dashboard</h1>
    </div>
    <div class="container">
        <div class="card" style="border:1px solid #00bcd4; background:#16262e; display:flex; justify-content:space-between; align-items:center;">
            <div>
                <h2 style="margin:0; border:none; padding:0; color:#00e5ff;">📁 LittleFS File Explorer</h2>
                <div style="margin-top:4px; font-size:0.85em; color:#aaa;">Manage, download & upload files directly on the watch</div>
            </div>
            <a href="/fm" style="text-decoration:none;"><button type="button" style="width:auto; margin:0; padding:10px 18px; background:#00bcd4; font-size:0.95em; font-weight:bold;">Open Explorer</button></a>
        </div>
        <div class="card">
            <h2>Device Status</h2>
            <div class="status-row"><span>Wi-Fi</span><span id="st_wifi">Loading...</span></div>
            <div class="status-row"><span>IP Address</span><span id="st_ip">...</span></div>
            <div class="status-row"><span>Signal</span><span id="st_rssi">...</span></div>
            <div class="status-row"><span>Time Sync</span><span id="st_time">...</span></div>
            <div class="status-row"><span>Weather Sync</span><span id="st_wsync">...</span></div>
            <div class="status-row"><span>Last Weather</span><span id="st_wlast">...</span></div>
            <div class="status-row"><span>Uptime</span><span id="st_up">...</span></div>
        </div>
        <form action="/save" method="POST">
            <div class="card">
                <h2>Wi-Fi Configuration</h2>
                <label>SSID</label>
                <input type="text" id="ssid" name="ssid" value="{{SSID}}">
                <button type="button" class="secondary-btn" onclick="scanWifi()">Scan Networks</button>
                <div id="scanResults"></div>
                <label>Password (leave blank to keep current)</label>
                <input type="password" name="pass" placeholder="********">
            </div>
            <div class="card">
                <h2>Time & Weather</h2>
                <label>Timezone (POSIX format)</label>
                <input type="text" name="tz" value="{{TZ}}">
                <label>OpenWeatherMap API Key (leave blank to keep current)</label>
                <input type="password" name="owm_key" placeholder="********">
                <label>City Location (e.g., London,UK)</label>
                <input type="text" name="owm_loc" value="{{OWM_LOC}}">
                <label>Latitude & Longitude</label>
                <div style="display:flex; gap:10px;">
                    <input type="number" step="0.0001" name="lat" value="{{LAT}}">
                    <input type="number" step="0.0001" name="lon" value="{{LON}}">
                </div>
                <label>Units</label>
                <select name="owm_unt">
                    <option value="metric" {{OPT_METRIC}}>Metric (deg C, m/s)</option>
                    <option value="imperial" {{OPT_IMPERIAL}}>Imperial (deg F, mph)</option>
                </select>
                <label>Update Interval (minutes)</label>
                <input type="number" name="w_int" value="{{W_INT}}">
                <button type="button" class="secondary-btn" onclick="forceWeather()">Force Weather Update Now</button>
            </div>
            <button type="submit">Save & Reboot</button>
        </form>
    </div>
</body>
</html>)rawliteral";

String WifiPortal::getHtml() {
    AppConfig& cfg = configManager.get();
    String html = FPSTR(DASHBOARD_HTML_TEMPLATE);
    html.replace("{{SSID}}", cfg.wifi_ssid);
    html.replace("{{TZ}}", cfg.timezone);
    html.replace("{{OWM_LOC}}", cfg.owm_location);
    html.replace("{{LAT}}", String(cfg.latitude, 4));
    html.replace("{{LON}}", String(cfg.longitude, 4));
    html.replace("{{OPT_METRIC}}", cfg.owm_units == "metric" ? "selected" : "");
    html.replace("{{OPT_IMPERIAL}}", cfg.owm_units == "imperial" ? "selected" : "");
    html.replace("{{W_INT}}", String(cfg.weather_interval_ms / 60000));
    return html;
}

#include "file_manager.h"

int WifiPortal::countFilesRecursive(const String& path) {
    int count = 0;
    FileInfo entries[32];
    size_t num = fileManager.listDir(path, entries, 32);
    for (size_t i = 0; i < num; i++) {
        if (entries[i].isDir) {
            String sub = path;
            if (!sub.endsWith("/")) sub += "/";
            sub += entries[i].name;
            count += countFilesRecursive(sub);
        } else {
            count++;
        }
    }
    return count;
}

int WifiPortal::getTotalFileCount() {
    return countFilesRecursive("/");
}

static const char FILE_MANAGER_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><title>Q-Watch File Manager</title><style>
body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#121212;color:#eee;margin:0;padding:20px;}
.container{max-width:750px;margin:auto;background:#1e1e1e;padding:20px;border-radius:8px;box-shadow:0 4px 10px rgba(0,0,0,0.5);}
h1{color:#00bcd4;margin-top:0;border-bottom:1px solid #333;padding-bottom:10px;display:flex;justify-content:space-between;align-items:center;}
.row{display:flex;justify-content:space-between;align-items:center;padding:10px;border-bottom:1px solid #2a2a2a;}
.row:hover{background:#252525;}
button{background:#00bcd4;color:#fff;border:none;padding:7px 12px;border-radius:4px;cursor:pointer;margin-left:4px;font-size:0.9em;}
button:hover{filter:brightness(1.15);}
button.del{background:#f44336;}
button.sec{background:#444;}
button.ren{background:#ff9800;}
button.cpy{background:#9c27b0;}
input[type=file]{color:#aaa;}
.actions{display:flex;gap:4px;align-items:center;}
</style><script>
let curPath='/';
function loadFiles(path){curPath=path;fetch('/file_list?path='+encodeURIComponent(path)).then(r=>r.json()).then(data=>{
let list=document.getElementById('list');list.innerHTML='';
document.getElementById('path').innerText=data.path;
if(path!=='/'){
let up=path.substring(0,path.lastIndexOf('/'));if(!up)up='/';
let upDiv=document.createElement('div');upDiv.className='row';
upDiv.innerHTML="<span style='cursor:pointer;color:#00bcd4;' onclick=\"loadFiles('"+up+"')\">⬆ .. (Parent Directory)</span><span></span>";
list.appendChild(upDiv);
}
data.entries.forEach(item=>{
let d=document.createElement('div');d.className='row';
let fullPath=(path=='/'?'':path)+'/'+item.name;
let name=item.isDir?"<span style='cursor:pointer;color:#ffb74d;font-weight:bold;' onclick=\"loadFiles('"+fullPath+"')\">📁 "+item.name+"/</span>":"<span>📄 "+item.name+"</span>";
let size=item.isDir?'DIR':(item.size+' B');
let btns="<div class='actions'>";
if(item.isDir){
btns+="<button onclick=\"loadFiles('"+fullPath+"')\">Open</button>";
}else{
btns+="<button onclick=\"location.href='/file_download?path="+encodeURIComponent(fullPath)+"'\">Download</button>";
btns+="<button class='cpy' onclick=\"copyFile('"+fullPath+"')\">Copy</button>";
}
btns+="<button class='ren' onclick=\"renFile('"+fullPath+"')\">Rename</button>";
btns+="<button class='del' onclick=\"delFile('"+fullPath+"')\">Delete</button></div>";
d.innerHTML="<div>"+name+" <small style='color:#888;margin-left:8px;'>"+size+"</small></div>"+btns;
list.appendChild(d);});
if(data.entries.length===0&&path==='/'){list.innerHTML="<div style='padding:20px;text-align:center;color:#888;'>(Directory is empty)</div>";}
}).catch(e=>{document.getElementById('list').innerHTML="<div style='color:#f44336;'>Error loading files</div>";});}
function delFile(p){if(confirm('Delete '+p+'?')){fetch('/file_delete?path='+encodeURIComponent(p),{method:'POST'}).then(()=>loadFiles(curPath));}}
function renFile(p){let n=prompt('Rename '+p+' to:',p);if(n&&n!==p){fetch('/file_rename?from='+encodeURIComponent(p)+'&to='+encodeURIComponent(n),{method:'POST'}).then(()=>loadFiles(curPath));}}
function copyFile(p){let n=prompt('Copy '+p+' to:',p+'.copy');if(n&&n!==p){fetch('/file_copy?from='+encodeURIComponent(p)+'&to='+encodeURIComponent(n),{method:'POST'}).then(()=>loadFiles(curPath));}}
function uploadFile(){let f=document.getElementById('f').files[0];if(!f)return;
let formData=new FormData();formData.append('data',f,curPath=='/'?'/'+f.name:curPath+'/'+f.name);
fetch('/file_upload',{method:'POST',body:formData}).then(()=>{document.getElementById('f').value='';loadFiles(curPath);});}
function mkDir(){let n=prompt('New Folder Name:');if(n){fetch('/file_mkdir?path='+encodeURIComponent((curPath=='/'?'':curPath)+'/'+n),{method:'POST'}).then(()=>loadFiles(curPath));}}
window.onload=()=>loadFiles('/');
</script></head><body><div class='container'>
<h1><span>Q-Watch LittleFS Explorer</span><button class='sec' onclick="location.href='/'">Dashboard</button></h1>
<h3>Path: <span id='path' style='color:#00bcd4;'>/</span></h3>
<div style='margin-bottom:15px;display:flex;gap:8px;'>
<button onclick="loadFiles('/')">Root /</button>
<button onclick="mkDir()">+ New Folder</button>
<button class='sec' onclick="loadFiles(curPath)">Refresh</button>
</div>
<div id='list'></div>
<div style='margin-top:20px;border-top:1px solid #333;padding-top:15px;'>
<strong>Upload to Current Directory:</strong><br><br>
<input type='file' id='f'><button onclick='uploadFile()'>Upload File</button>
</div>
</div></body></html>)rawliteral";

void WifiPortal::handleFileManagerGui() {
    if (!settingsManager.get().fileserver_enabled) {
        settingsManager.get().fileserver_enabled = true;
        settingsManager.save();
    }
    server.send_P(200, "text/html", FILE_MANAGER_HTML);
}

void WifiPortal::handleFileList() {
    if (!settingsManager.get().fileserver_enabled) {
        settingsManager.get().fileserver_enabled = true;
        settingsManager.save();
    }
    String path = server.hasArg("path") ? server.arg("path") : "/";
    if (!FileManager::isPathSafe(path)) {
        server.send(400, "application/json", "{\"error\":\"Invalid / Unsafe Path\"}");
        return;
    }
    FileInfo entries[32];
    size_t num = fileManager.listDir(path, entries, 32);

    String json;
    json.reserve(num * 64 + 64);
    json = "{\"path\":\"" + path + "\",\"entries\":[";
    for (size_t i = 0; i < num; i++) {
        if (i > 0) json += ",";
        json += "{\"name\":\"" + entries[i].name + "\",\"size\":" + String(entries[i].size) + ",\"isDir\":" + String(entries[i].isDir ? "true" : "false") + "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void WifiPortal::handleFileDownload() {
    if (!settingsManager.get().fileserver_enabled) {
        settingsManager.get().fileserver_enabled = true;
        settingsManager.save();
    }
    if (!server.hasArg("path")) {
        server.send(400, "text/plain", "Missing Path");
        return;
    }
    String path = server.arg("path");
    if (!FileManager::isPathSafe(path)) {
        server.send(400, "text/plain", "Invalid / Unsafe Path");
        return;
    }
    if (!fileManager.exists(path)) {
        server.send(404, "text/plain", "File Not Found");
        return;
    }

    File file = LittleFS.open(fileManager.normalizePath(path), FILE_READ);
    server.streamFile(file, "application/octet-stream");
    file.close();
}

void WifiPortal::handleFileDelete() {
    if (!settingsManager.get().fileserver_enabled) {
        settingsManager.get().fileserver_enabled = true;
        settingsManager.save();
    }
    if (server.hasArg("path")) {
        String path = server.arg("path");
        if (!FileManager::isPathSafe(path)) {
            server.send(400, "text/plain", "Invalid / Unsafe Path");
            return;
        }
        fileManager.remove(path);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing Path");
    }
}

void WifiPortal::handleFileMkdir() {
    if (!settingsManager.get().fileserver_enabled) {
        settingsManager.get().fileserver_enabled = true;
        settingsManager.save();
    }
    if (server.hasArg("path")) {
        String p = server.arg("path");
        if (!FileManager::isPathSafe(p)) {
            server.send(400, "text/plain", "Invalid / Unsafe Path");
            return;
        }
        if (!p.endsWith("/")) p += "/.keep";
        FileManager::ensureParentDir(p);
        fileManager.create(p);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing Path");
    }
}

void WifiPortal::handleFileRename() {
    if (!settingsManager.get().fileserver_enabled) {
        settingsManager.get().fileserver_enabled = true;
        settingsManager.save();
    }
    if (server.hasArg("from") && server.hasArg("to")) {
        String from = server.arg("from");
        String to = server.arg("to");
        if (!FileManager::isPathSafe(from) || !FileManager::isPathSafe(to)) {
            server.send(400, "text/plain", "Invalid / Unsafe Path");
            return;
        }
        FileManager::ensureParentDir(to);
        if (fileManager.rename(from, to)) {
            server.send(200, "text/plain", "OK");
        } else {
            server.send(500, "text/plain", "Rename Failed");
        }
    } else {
        server.send(400, "text/plain", "Missing Params");
    }
}

void WifiPortal::handleFileCopy() {
    if (!settingsManager.get().fileserver_enabled) {
        settingsManager.get().fileserver_enabled = true;
        settingsManager.save();
    }
    if (server.hasArg("from") && server.hasArg("to")) {
        String from = server.arg("from");
        String to = server.arg("to");
        if (!FileManager::isPathSafe(from) || !FileManager::isPathSafe(to) || !fileManager.exists(from)) {
            server.send(400, "text/plain", "Invalid / Unsafe Path or Not Found");
            return;
        }
        FileManager::ensureParentDir(to);
        File fIn = LittleFS.open(fileManager.normalizePath(from), FILE_READ);
        if (!fIn) {
            server.send(500, "text/plain", "Failed to open source file");
            return;
        }
        File fOut = LittleFS.open(fileManager.normalizePath(to), FILE_WRITE);
        if (!fOut) {
            fIn.close();
            server.send(500, "text/plain", "Failed to create target file");
            return;
        }
        uint8_t buf[256];
        while (fIn.available()) {
            size_t n = fIn.read(buf, sizeof(buf));
            fOut.write(buf, n);
        }
        fIn.close();
        fOut.close();
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing Params");
    }
}

static File uploadFile;

void WifiPortal::handleFileUpload() {
    if (!settingsManager.get().fileserver_enabled) {
        settingsManager.get().fileserver_enabled = true;
        settingsManager.save();
    }

    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!FileManager::isPathSafe(filename)) {
            return; // Reject unsafe upload filename
        }
        String p = fileManager.normalizePath(filename);
        FileManager::ensureParentDir(p);
        uploadFile = LittleFS.open(p, FILE_WRITE);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
        }
    }
}

String WifiPortal::getFileManagerHtml() {
    return String(reinterpret_cast<const __FlashStringHelper*>(FILE_MANAGER_HTML));
}
