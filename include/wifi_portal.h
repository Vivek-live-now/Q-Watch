#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

enum class WifiState {
    OFF,
    NO_CREDS,
    CONNECTING,
    CONNECTED,
    FAILED,
    DISCONNECTED,
    PORTAL
};

struct ScannedNetwork {
    String ssid;
    int rssi;
    bool encrypted;
};

class WifiPortal {
public:
    WifiPortal();
    void begin();
    void loop();
    WifiState getState();

    void enableWifi();
    void disableWifi();
    const char* getDetailedStatusStr();
    String getSSID();
    String getIP();

    // Wi-Fi Scanner API
    void startScan();
    bool isScanning() const { return scan_in_progress; }
    int getScannedNetworkCount() const { return scanned_count; }
    const ScannedNetwork* getScannedNetworks() const { return scanned_networks; }

    // Direct Connection API
    void connectToNetwork(const String& ssid, const String& password);

    // File Server Helper Queries
    int getTotalFileCount();

private:
    WebServer server;
    DNSServer dnsServer;
    WifiState state;
    uint32_t connect_start_time;
    uint32_t last_reconnect_attempt;
    bool scan_in_progress;

    ScannedNetwork scanned_networks[16];
    int scanned_count;

    void startPortal();
    void setupRoutes();
    void handleRoot();
    void handleSave();
    void handleScanTrigger();
    void handleScanResults();
    void handleStatusJson();
    void handleWeatherForce();

    // Web File Manager Endpoints
    void handleFileManagerGui();
    void handleFileList();
    void handleFileUpload();
    void handleFileDownload();
    void handleFileDelete();
    void handleFileMkdir();
    void handleFileRename();

    String getHtml();
    String getFileManagerHtml();
    int countFilesRecursive(const String& path);
};

extern WifiPortal wifiPortal;

#endif
