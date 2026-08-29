#include "config.h"

ConfigManager configManager;

ConfigManager::ConfigManager() {
    // Set defaults
    config.timezone = "IST-5:30"; // Default Asia/Kolkata
    config.latitude = 28.6139;    // Default New Delhi
    config.longitude = 77.2090;
    config.owm_api_key = "";
    config.owm_location = "New Delhi,IN";
    config.owm_units = "metric";
    config.weather_interval_ms = 30 * 60 * 1000; // 30 minutes default for OWM to save calls
}

void ConfigManager::load() {
    preferences.begin("qwatch", true); // true = read-only
    config.wifi_ssid = preferences.getString("ssid", "");
    config.wifi_password = preferences.getString("pass", "");
    config.timezone = preferences.getString("tz", "IST-5:30");
    config.latitude = preferences.getFloat("lat", 28.6139);
    config.longitude = preferences.getFloat("lon", 77.2090);
    config.owm_api_key = preferences.getString("owm_key", "");
    config.owm_location = preferences.getString("owm_loc", "New Delhi,IN");
    config.owm_units = preferences.getString("owm_unt", "metric");
    config.weather_interval_ms = preferences.getUInt("w_int", 30 * 60 * 1000);
    preferences.end();
}

void ConfigManager::save() {
    preferences.begin("qwatch", false); // false = read-write
    preferences.putString("ssid", config.wifi_ssid);
    preferences.putString("pass", config.wifi_password);
    preferences.putString("tz", config.timezone);
    preferences.putFloat("lat", config.latitude);
    preferences.putFloat("lon", config.longitude);
    preferences.putString("owm_key", config.owm_api_key);
    preferences.putString("owm_loc", config.owm_location);
    preferences.putString("owm_unt", config.owm_units);
    preferences.putUInt("w_int", config.weather_interval_ms);
    preferences.end();
}

AppConfig& ConfigManager::get() {
    return config;
}

void ConfigManager::set(const AppConfig& new_config) {
    // Basic optimization: could check if changed before saving, but handled by caller logic
    config = new_config;
}
