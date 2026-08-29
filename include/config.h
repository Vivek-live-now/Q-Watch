#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

struct AppConfig {
    String wifi_ssid;
    String wifi_password;
    String timezone;
    String owm_api_key;
    String owm_location; // e.g. "London,UK"
    String owm_units;    // "metric" or "imperial"
    float latitude;
    float longitude;
    uint32_t weather_interval_ms;
};

class ConfigManager {
public:
    ConfigManager();
    void load();
    void save();

    AppConfig& get();
    void set(const AppConfig& new_config);

private:
    Preferences preferences;
    AppConfig config;
};

extern ConfigManager configManager;

#endif
