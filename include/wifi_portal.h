#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

enum class WifiState {
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    PORTAL
};

class WifiPortal {
public:
    WifiPortal();
    void begin();
    void loop();
    WifiState getState();

private:
    WebServer server;
    DNSServer dnsServer;
    WifiState state;
    uint32_t connect_start_time;
    uint32_t last_reconnect_attempt;
    bool scan_in_progress;

    void startPortal();
    void setupRoutes();
    void handleRoot();
    void handleSave();
    void handleScanTrigger();
    void handleScanResults();
    void handleStatusJson();
    void handleWeatherForce();
    String getHtml();
};

extern WifiPortal wifiPortal;

#endif
