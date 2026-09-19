#include "ui_core.h"
#include "wifi_portal.h"
#include "config.h"
#include "weather.h"
#include "clock.h"
#include "settings_data.h"
#include <ESPmDNS.h>

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

void WifiPortal::enableWifi() {
    AppConfig& cfg = configManager.get();
    WiFi.hostname("Q-Watch");

    if (cfg.wifi_ssid.length() > 0) {
        Serial.print("Connecting to Wi-Fi: ");
        Serial.println(cfg.wifi_ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_password.c_str());
        state = WifiState::CONNECTING;
        connect_start_time = millis();
    } else {
        Serial.println("Wi-Fi enabled but no credentials found.");
        WiFi.mode(WIFI_STA);
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

    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    setupRoutes();
    server.begin();
    Serial.println("Portal running.");
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
    server.on("/file_upload", HTTP_POST, [this]() {
        server.send(200, "text/plain", "Upload Successful");
    }, std::bind(&WifiPortal::handleFileUpload, this));

    server.onNotFound([this]() {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.send(302, "text/plain", "");
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
    return "-";
}

void WifiPortal::handleRoot() {
    server.send(200, "text/html", getHtml());
}

void WifiPortal::handleStatusJson() {
    String json = "{";
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
        String json = "{\"status\":\"complete\",\"networks\":[";
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

String WifiPortal::getHtml() {
    AppConfig& cfg = configManager.get();
    String html = "<!DOCTYPE html>\n";
    html += "<html>\n";
    html += "<head>\n";
    html += "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
    html += "    <title>Q-Watch Dashboard</title>\n";
    html += "    <style>\n";
    html += "        body { font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, Helvetica, Arial, sans-serif; background: #121212; color: #f0f0f0; margin: 0; padding: 0; }\n";
    html += "        .header { background: #1e1e1e; padding: 20px; text-align: center; border-bottom: 2px solid #333; }\n";
    html += "        .container { padding: 20px; max-width: 600px; margin: auto; }\n";
    html += "        .card { background: #1e1e1e; border-radius: 8px; padding: 15px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }\n";
    html += "        h2 { margin-top: 0; border-bottom: 1px solid #333; padding-bottom: 10px; font-size: 1.2em; color: #00bcd4; }\n";
    html += "        label { display: block; margin-top: 10px; font-size: 0.9em; color: #aaa; }\n";
    html += "        input, select { width: 100%; box-sizing: border-box; padding: 10px; margin-top: 5px; background: #2c2c2c; color: #fff; border: 1px solid #444; border-radius: 4px; }\n";
    html += "        button { background: #00bcd4; color: #fff; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; font-weight: bold; width: 100%; margin-top: 15px; }\n";
    html += "        button:hover { background: #0097a7; }\n";
    html += "        .secondary-btn { background: #444; margin-top: 5px; }\n";
    html += "        .secondary-btn:hover { background: #555; }\n";
    html += "        .status-row { display: flex; justify-content: space-between; padding: 5px 0; border-bottom: 1px solid #333; font-size: 0.9em; }\n";
    html += "        .net-item { padding: 10px; background: #2c2c2c; margin-bottom: 5px; border-radius: 4px; cursor: pointer; display: flex; justify-content: space-between; }\n";
    html += "        .net-item:hover { background: #3c3c3c; }\n";
    html += "        #scanResults { max-height: 200px; overflow-y: auto; margin-top: 10px; display: none; }\n";
    html += "    </style>\n";
    html += "    <script>\n";
    html += "        function fetchStatus() {\n";
    html += "            fetch('/status_json').then(r=>r.json()).then(data => {\n";
    html += "                document.getElementById('st_wifi').innerText = data.wifi;\n";
    html += "                document.getElementById('st_ip').innerText = data.ip;\n";
    html += "                document.getElementById('st_rssi').innerText = data.rssi + \" dBm\";\n";
    html += "                document.getElementById('st_time').innerText = data.time_sync;\n";
    html += "                document.getElementById('st_wsync').innerText = data.w_sync;\n";
    html += "                document.getElementById('st_wlast').innerText = data.w_last;\n";
    html += "                document.getElementById('st_up').innerText = data.uptime + \" s\";\n";
    html += "            }).catch(e => console.error(e));\n";
    html += "        }\n";
    html += "        function pollScanResults() {\n";
    html += "            fetch('/scan_results').then(r=>r.json()).then(data => {\n";
    html += "                let res = document.getElementById('scanResults');\n";
    html += "                if (data.status === 'running') {\n";
    html += "                    setTimeout(pollScanResults, 1000);\n";
    html += "                } else if (data.status === 'complete') {\n";
    html += "                    res.innerHTML = \"\";\n";
    html += "                    data.networks.forEach(net => {\n";
    html += "                        let d = document.createElement('div');\n";
    html += "                        d.className = 'net-item';\n";
    html += "                        d.innerHTML = `<span>${net.ssid} ${net.enc ? 'SECURE' : 'OPEN'}</span><span>${net.rssi} dBm</span>`;\n";
    html += "                        d.onclick = () => { document.getElementById('ssid').value = net.ssid; res.style.display='none'; };\n";
    html += "                        res.appendChild(d);\n";
    html += "                    });\n";
    html += "                } else {\n";
    html += "                    res.innerHTML = \"Scan failed.\";\n";
    html += "                }\n";
    html += "            }).catch(e => { document.getElementById('scanResults').innerHTML = \"Scan error.\"; });\n";
    html += "        }\n";
    html += "        function scanWifi() {\n";
    html += "            let res = document.getElementById('scanResults');\n";
    html += "            res.style.display = 'block';\n";
    html += "            res.innerHTML = \"Triggering Async Scan...\";\n";
    html += "            fetch('/scan_trigger').then(() => {\n";
    html += "                res.innerHTML = \"Scanning in background...\";\n";
    html += "                setTimeout(pollScanResults, 1000);\n";
    html += "            });\n";
    html += "        }\n";
    html += "        function forceWeather() {\n";
    html += "            fetch('/weather_force').then(() => alert('Weather update triggered.'));\n";
    html += "        }\n";
    html += "        setInterval(fetchStatus, 5000);\n";
    html += "        window.onload = fetchStatus;\n";
    html += "    </script>\n";
    html += "</head>\n";
    html += "<body>\n";
    html += "    <div class=\"header\">\n";
    html += "        <h1>Q-Watch Dashboard</h1>\n";
    html += "    </div>\n";
    html += "    <div class=\"container\">\n";
    html += "        <div class=\"card\">\n";
    html += "            <h2>Device Status</h2>\n";
    html += "            <div class=\"status-row\"><span>Wi-Fi</span><span id=\"st_wifi\">Loading...</span></div>\n";
    html += "            <div class=\"status-row\"><span>IP Address</span><span id=\"st_ip\">...</span></div>\n";
    html += "            <div class=\"status-row\"><span>Signal</span><span id=\"st_rssi\">...</span></div>\n";
    html += "            <div class=\"status-row\"><span>Time Sync</span><span id=\"st_time\">...</span></div>\n";
    html += "            <div class=\"status-row\"><span>Weather Sync</span><span id=\"st_wsync\">...</span></div>\n";
    html += "            <div class=\"status-row\"><span>Last Weather</span><span id=\"st_wlast\">...</span></div>\n";
    html += "            <div class=\"status-row\"><span>Uptime</span><span id=\"st_up\">...</span></div>\n";
    html += "        </div>\n";
    html += "        <form action=\"/save\" method=\"POST\">\n";
    html += "            <div class=\"card\">\n";
    html += "                <h2>Wi-Fi Configuration</h2>\n";
    html += "                <label>SSID</label>\n";
    html += "                <input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"" + cfg.wifi_ssid + "\">\n";
    html += "                <button type=\"button\" class=\"secondary-btn\" onclick=\"scanWifi()\">Scan Networks</button>\n";
    html += "                <div id=\"scanResults\"></div>\n";
    html += "                <label>Password (leave blank to keep current)</label>\n";
    html += "                <input type=\"password\" name=\"pass\" placeholder=\"********\">\n";
    html += "            </div>\n";
    html += "            <div class=\"card\">\n";
    html += "                <h2>Time & Weather</h2>\n";
    html += "                <label>Timezone (POSIX format)</label>\n";
    html += "                <input type=\"text\" name=\"tz\" value=\"" + cfg.timezone + "\">\n";
    html += "                <label>OpenWeatherMap API Key (leave blank to keep current)</label>\n";
    html += "                <input type=\"password\" name=\"owm_key\" placeholder=\"********\">\n";
    html += "                <label>City Location (e.g., London,UK)</label>\n";
    html += "                <input type=\"text\" name=\"owm_loc\" value=\"" + cfg.owm_location + "\">\n";
    html += "                <label>Latitude & Longitude</label>\n";
    html += "                <div style=\"display:flex; gap:10px;\">\n";
    html += "                    <input type=\"number\" step=\"0.0001\" name=\"lat\" value=\"" + String(cfg.latitude, 4) + "\">\n";
    html += "                    <input type=\"number\" step=\"0.0001\" name=\"lon\" value=\"" + String(cfg.longitude, 4) + "\">\n";
    html += "                </div>\n";
    html += "                <label>Units</label>\n";
    html += "                <select name=\"owm_unt\">\n";
    html += "                    <option value=\"metric\" " + String(cfg.owm_units == "metric" ? "selected" : "") + ">Metric (deg C, m/s)</option>\n";
    html += "                    <option value=\"imperial\" " + String(cfg.owm_units == "imperial" ? "selected" : "") + ">Imperial (deg F, mph)</option>\n";
    html += "                </select>\n";
    html += "                <label>Update Interval (minutes)</label>\n";
    html += "                <input type=\"number\" name=\"w_int\" value=\"" + String(cfg.weather_interval_ms / 60000) + "\">\n";
    html += "                <button type=\"button\" class=\"secondary-btn\" onclick=\"forceWeather()\">Force Weather Update Now</button>\n";
    html += "            </div>\n";
    html += "            <button type=\"submit\">Save & Reboot</button>\n";
    html += "        </form>\n";
    html += "    </div>\n";
    html += "</body>\n";
    html += "</html>\n";

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

void WifiPortal::handleFileManagerGui() {
    if (!settingsManager.get().fileserver_enabled) {
        server.send(403, "text/plain", "File Server Disabled in Settings");
        return;
    }
    server.send(200, "text/html", getFileManagerHtml());
}

void WifiPortal::handleFileList() {
    if (!settingsManager.get().fileserver_enabled) {
        server.send(403, "application/json", "{\"error\":\"File Server Disabled\"}");
        return;
    }
    String path = server.hasArg("path") ? server.arg("path") : "/";
    FileInfo entries[32];
    size_t num = fileManager.listDir(path, entries, 32);

    String json = "{\"path\":\"" + path + "\",\"entries\":[";
    for (size_t i = 0; i < num; i++) {
        if (i > 0) json += ",";
        json += "{\"name\":\"" + entries[i].name + "\",\"size\":" + String(entries[i].size) + ",\"isDir\":" + String(entries[i].isDir ? "true" : "false") + "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void WifiPortal::handleFileDownload() {
    if (!settingsManager.get().fileserver_enabled) {
        server.send(403, "text/plain", "File Server Disabled");
        return;
    }
    if (!server.hasArg("path")) {
        server.send(400, "text/plain", "Missing Path");
        return;
    }
    String path = server.arg("path");
    if (!fileManager.exists(path)) {
        server.send(404, "text/plain", "File Not Found");
        return;
    }

    File file = LittleFS.open(path, FILE_READ);
    server.streamFile(file, "application/octet-stream");
    file.close();
}

void WifiPortal::handleFileDelete() {
    if (!settingsManager.get().fileserver_enabled) {
        server.send(403, "text/plain", "File Server Disabled");
        return;
    }
    if (server.hasArg("path")) {
        fileManager.remove(server.arg("path"));
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing Path");
    }
}

void WifiPortal::handleFileMkdir() {
    if (!settingsManager.get().fileserver_enabled) {
        server.send(403, "text/plain", "File Server Disabled");
        return;
    }
    if (server.hasArg("path")) {
        String p = server.arg("path");
        if (!p.endsWith("/")) p += "/.keep";
        fileManager.create(p);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing Path");
    }
}

void WifiPortal::handleFileRename() {
    if (!settingsManager.get().fileserver_enabled) {
        server.send(403, "text/plain", "File Server Disabled");
        return;
    }
    if (server.hasArg("from") && server.hasArg("to")) {
        fileManager.rename(server.arg("from"), server.arg("to"));
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing Params");
    }
}

static File uploadFile;

void WifiPortal::handleFileUpload() {
    if (!settingsManager.get().fileserver_enabled) return;

    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) filename = "/" + filename;
        uploadFile = LittleFS.open(filename, FILE_WRITE);
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
    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Q-Watch File Manager</title><style>";
    html += "body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#121212;color:#eee;margin:0;padding:20px;}";
    html += ".container{max-width:700px;margin:auto;background:#1e1e1e;padding:20px;border-radius:8px;box-shadow:0 4px 10px rgba(0,0,0,0.5);}";
    html += "h1{color:#00bcd4;margin-top:0;border-bottom:1px solid #333;padding-bottom:10px;}";
    html += ".row{display:flex;justify-content:space-between;align-items:center;padding:10px;border-bottom:1px solid #2a2a2a;}";
    html += "button{background:#00bcd4;color:#fff;border:none;padding:8px 12px;border-radius:4px;cursor:pointer;margin-left:5px;}";
    html += "button.del{background:#f44336;} input[type=file]{color:#aaa;}";
    html += "</style><script>";
    html += "let curPath='/';";
    html += "function loadFiles(path){curPath=path;fetch('/file_list?path='+encodeURIComponent(path)).then(r=>r.json()).then(data=>{";
    html += "let list=document.getElementById('list');list.innerHTML='';";
    html += "document.getElementById('path').innerText=data.path;";
    html += "data.entries.forEach(item=>{";
    html += "let d=document.createElement('div');d.className='row';";
    html += "let name=item.isDir?'📁 '+item.name+'/':'📄 '+item.name;";
    html += "let size=item.isDir?'':(item.size+' B');";
    html += "let btns=item.isDir?\"<button onclick=\\\"loadFiles('\"+(path=='/'?'':path)+\"/\"+item.name+\"')\\\">Open</button>\":\"<button onclick=\\\"location.href='/file_download?path=\"+encodeURIComponent((path=='/'?'':path)+'/'+item.name)+\"'\\\">Download</button>\";";
    html += "btns+=\"<button class='del' onclick=\\\"delFile('\"+(path=='/'?'':path)+\"/\"+item.name+\"')\\\">Delete</button>\";";
    html += "d.innerHTML=\"<span>\"+name+\"</span><span>\"+size+\" \"+btns+\"</span>\";";
    html += "list.appendChild(d);});";
    html += "});}";
    html += "function delFile(p){if(confirm('Delete '+p+'?')){fetch('/file_delete?path='+encodeURIComponent(p),{method:'POST'}).then(()=>loadFiles(curPath));}}";
    html += "function uploadFile(){let f=document.getElementById('f').files[0];if(!f)return;";
    html += "let formData=new FormData();formData.append('data',f,curPath=='/'?'/'+f.name:curPath+'/'+f.name);";
    html += "fetch('/file_upload',{method:'POST',body:formData}).then(()=>loadFiles(curPath));}";
    html += "function mkDir(){let n=prompt('New Folder Name:');if(n){fetch('/file_mkdir?path='+encodeURIComponent((curPath=='/'?'':curPath)+'/'+n),{method:'POST'}).then(()=>loadFiles(curPath));}}";
    html += "window.onload=()=>loadFiles('/');";
    html += "</script></head><body><div class='container'><h1>Q-Watch File Manager</h1>";
    html += "<h3>Path: <span id='path'>/</span></h3>";
    html += "<div style='margin-bottom:15px;'><button onclick=\"loadFiles('/')\">Root</button><button onclick=\"mkDir()\">New Folder</button></div>";
    html += "<div id='list'></div>";
    html += "<div style='margin-top:20px;border-top:1px solid #333;padding-top:15px;'><input type='file' id='f'><button onclick='uploadFile()'>Upload File</button></div>";
    html += "</div></body></html>";
    return html;
}
