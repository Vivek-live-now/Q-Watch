#include "weather.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

Weather weather;

Weather::Weather() {
    data.valid = false;
    last_update_time = 0;
    needs_update = true;
}

void Weather::loop() {
    if (WiFi.status() != WL_CONNECTED) return;

    AppConfig& cfg = configManager.get();

    // Check if it's time for an update
    if (needs_update || (millis() - last_update_time > cfg.weather_interval_ms)) {
        // ALWAYS update the timer so we don't spam the API on failure
        last_update_time = millis();
        needs_update = false;

        fetchWeather();
    }
}

void Weather::forceUpdate() {
    needs_update = true;
}

const WeatherData& Weather::getData() const {
    return data;
}

String Weather::mapWmoCodeToCondition(int code) {
    if (code == 0) return "Clear";
    if (code == 1 || code == 2 || code == 3) return "Clouds";
    if (code == 45 || code == 48) return "Fog";
    if (code >= 51 && code <= 55) return "Drizzle";
    if (code >= 61 && code <= 67) return "Rain";
    if (code >= 71 && code <= 77) return "Snow";
    if (code >= 80 && code <= 82) return "Showers";
    if (code >= 95 && code <= 99) return "Storm";
    return "Unknown";
}

// In a real production watch, we would use AsyncHTTPRequest or an RTOS task here.
// For this implementation, we use a very short timeout to minimize blocking.
void Weather::fetchWeather() {
    Serial.println("Fetching weather...");
    AppConfig& cfg = configManager.get();

    // Open-Meteo API doesn't require a key
    String url = "http://api.open-meteo.com/v1/forecast?latitude=" + String(cfg.latitude, 4) +
                 "&longitude=" + String(cfg.longitude, 4) +
                 "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m";

    HTTPClient http;
    http.setTimeout(2500); // Reduce timeout to 2.5s to minimize blocking on failure
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            data.temperature = doc["current"]["temperature_2m"];
            data.humidity = doc["current"]["relative_humidity_2m"];
            data.weather_code = doc["current"]["weather_code"];
            data.wind_speed = doc["current"]["wind_speed_10m"];
            data.valid = true;
            Serial.println("Weather update successful.");
        } else {
            Serial.println("JSON Parsing failed.");
        }
    } else {
        Serial.printf("Weather HTTP Request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}
