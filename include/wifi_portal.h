#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

enum class WifiState {
    CONNECTING,
    CONNECTED,
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

    void startPortal();
    void handleRoot();
    void handleSave();
    String getHtml();
};

extern WifiPortal wifiPortal;

#endif
