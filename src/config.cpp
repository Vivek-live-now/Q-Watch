#include "config.h"

ConfigManager configManager;

ConfigManager::ConfigManager() {
    // Set defaults
    config.timezone = "IST-5:30"; // Default Asia/Kolkata
    config.latitude = 28.6139;    // Default New Delhi
    config.longitude = 77.2090;
    config.weather_interval_ms = 15 * 60 * 1000; // 15 minutes
}

void ConfigManager::load() {
    preferences.begin("qwatch", true); // true = read-only
    config.wifi_ssid = preferences.getString("ssid", "");
    config.wifi_password = preferences.getString("pass", "");
    config.timezone = preferences.getString("tz", "IST-5:30");
    config.latitude = preferences.getFloat("lat", 28.6139);
    config.longitude = preferences.getFloat("lon", 77.2090);
    config.weather_interval_ms = preferences.getUInt("w_int", 15 * 60 * 1000);
    preferences.end();
}

void ConfigManager::save() {
    preferences.begin("qwatch", false); // false = read-write
    preferences.putString("ssid", config.wifi_ssid);
    preferences.putString("pass", config.wifi_password);
    preferences.putString("tz", config.timezone);
    preferences.putFloat("lat", config.latitude);
    preferences.putFloat("lon", config.longitude);
    preferences.putUInt("w_int", config.weather_interval_ms);
    preferences.end();
}

AppConfig& ConfigManager::get() {
    return config;
}

void ConfigManager::set(const AppConfig& new_config) {
    config = new_config;
}
