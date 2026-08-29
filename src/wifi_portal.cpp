#include "wifi_portal.h"
#include "config.h"

WifiPortal wifiPortal;

const byte DNS_PORT = 53;

WifiPortal::WifiPortal() : server(80), state(WifiState::CONNECTING) {}

void WifiPortal::begin() {
    configManager.load();
    AppConfig& cfg = configManager.get();

    if (cfg.wifi_ssid.length() > 0) {
        Serial.print("Connecting to Wi-Fi: ");
        Serial.println(cfg.wifi_ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifi_ssid.c_str(), cfg.wifi_password.c_str());
        state = WifiState::CONNECTING;
        connect_start_time = millis();
    } else {
        startPortal();
    }
}

void WifiPortal::startPortal() {
    Serial.println("Starting Captive Portal...");
    state = WifiState::PORTAL;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Q-Watch-Setup");

    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    server.on("/", HTTP_GET, std::bind(&WifiPortal::handleRoot, this));
    server.on("/save", HTTP_POST, std::bind(&WifiPortal::handleSave, this));
    server.onNotFound(std::bind(&WifiPortal::handleRoot, this));

    server.begin();
    Serial.println("Portal running.");
}

void WifiPortal::loop() {
    if (state == WifiState::CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Wi-Fi connected.");
            state = WifiState::CONNECTED;
        } else if (millis() - connect_start_time > 10000) {
            // 10 second timeout
            Serial.println("Wi-Fi connection failed. Falling back to Portal.");
            startPortal();
        }
    } else if (state == WifiState::PORTAL) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
}

WifiState WifiPortal::getState() {
    // If we are supposed to be connected but dropped, we can seamlessly reconnect in background
    // but the state remains CONNECTED for the UI unless we decide to reboot into AP mode.
    return state;
}

void WifiPortal::handleRoot() {
    server.send(200, "text/html", getHtml());
}

void WifiPortal::handleSave() {
    AppConfig cfg;
    cfg.wifi_ssid = server.arg("ssid");
    cfg.wifi_password = server.arg("pass");
    cfg.timezone = server.arg("tz");
    cfg.latitude = server.arg("lat").toFloat();
    cfg.longitude = server.arg("lon").toFloat();
    cfg.weather_interval_ms = server.arg("w_int").toInt() * 60 * 1000;

    configManager.set(cfg);
    configManager.save();

    server.send(200, "text/html", "<html><body><h1>Saved! Rebooting...</h1></body></html>");
    delay(1000);
    ESP.restart();
}

String WifiPortal::getHtml() {
    AppConfig& cfg = configManager.get();
    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<style>body{font-family:sans-serif;background:#222;color:#eee;padding:20px;} input,select{width:100%;padding:10px;margin:5px 0 15px 0;background:#333;color:#eee;border:1px solid #555;border-radius:5px;} input[type=\"submit\"]{background:#007BFF;font-weight:bold;}</style>";
    html += "</head><body><h2>Q-Watch Setup</h2>";
    html += "<form action=\"/save\" method=\"POST\">";
    html += "Wi-Fi SSID:<br><input type=\"text\" name=\"ssid\" value=\"" + cfg.wifi_ssid + "\"><br>";
    html += "Wi-Fi Password:<br><input type=\"password\" name=\"pass\" value=\"" + cfg.wifi_password + "\"><br>";
    html += "Timezone (POSIX):<br><input type=\"text\" name=\"tz\" value=\"" + cfg.timezone + "\"><br>";
    html += "Latitude:<br><input type=\"text\" name=\"lat\" value=\"" + String(cfg.latitude, 4) + "\"><br>";
    html += "Longitude:<br><input type=\"text\" name=\"lon\" value=\"" + String(cfg.longitude, 4) + "\"><br>";
    html += "Weather Interval (minutes):<br><input type=\"number\" name=\"w_int\" value=\"" + String(cfg.weather_interval_ms / 60000) + "\"><br>";
    html += "<input type=\"submit\" value=\"Save & Reboot\">";
    html += "</form></body></html>";
    return html;
}
