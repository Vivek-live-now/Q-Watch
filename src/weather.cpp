#include "weather.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

Weather weather;

Weather::Weather() {
    data.valid = false;
    last_update_time = 0;
    needs_update = true;
    in_progress = false;
    weatherTaskHandle = NULL;
}

void Weather::weatherTask(void *pvParameters) {
    Weather* instance = (Weather*)pvParameters;

    Serial.println("Fetching OpenWeatherMap data...");
    AppConfig& cfg = configManager.get();

    String url = "https://api.openweathermap.org/data/2.5/weather?lat=" + String(cfg.latitude, 4) +
                 "&lon=" + String(cfg.longitude, 4) +
                 "&units=" + cfg.owm_units +
                 "&appid=" + cfg.owm_api_key;

    // Use WiFiClientSecure for HTTPS connection
    WiFiClientSecure *client = new WiFiClientSecure;
    // For simplicity/compatibility in this environment, bypass strict certificate verification
    // Since this is just fetching weather, MITM risk is low.
    client->setInsecure();

    HTTPClient http;
    http.setTimeout(5000); // 5s timeout
    http.begin(*client, url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            instance->data.temperature = doc["main"]["temp"];
            instance->data.feels_like = doc["main"]["feels_like"];
            instance->data.humidity = doc["main"]["humidity"];
            instance->data.wind_speed = doc["wind"]["speed"];

            if (doc["weather"].size() > 0) {
                const char* desc = doc["weather"][0]["main"];
                instance->data.condition = String(desc);
            } else {
                instance->data.condition = "Unknown";
            }

            instance->data.valid = true;
            instance->last_update_time = millis(); // Set success time
            instance->needs_update = false;
            Serial.println("OWM HTTPS update successful.");
        } else {
            Serial.println("JSON Parsing failed.");
            instance->last_update_time = millis();
            instance->needs_update = false;
        }
    } else {
        Serial.printf("OWM HTTPS Request failed, error: %d\n", httpCode);
        instance->last_update_time = millis();
        instance->needs_update = false;
    }

    http.end();
    delete client;

    instance->in_progress = false;
    vTaskDelete(NULL);
}

void Weather::loop() {
    if (WiFi.status() != WL_CONNECTED || in_progress) return;

    AppConfig& cfg = configManager.get();

    if (cfg.owm_api_key.length() > 0) {
        if (needs_update || (millis() - last_update_time > cfg.weather_interval_ms)) {
            // Start weather fetch as a FreeRTOS task to prevent blocking the main loop
            in_progress = true;
            xTaskCreatePinnedToCore(
                weatherTask,
                "WeatherFetchTask",
                8192,           // Stack size
                this,           // Parameter
                1,              // Priority
                &weatherTaskHandle,
                0               // Core 0 (leaving core 1 for Arduino loop)
            );
        }
    }
}

void Weather::forceUpdate() {
    if (!in_progress) {
        needs_update = true;
    }
}

const WeatherData& Weather::getData() const {
    return data;
}

uint32_t Weather::getLastUpdateTime() const {
    return last_update_time;
}

bool Weather::isUpdateInProgress() const {
    return in_progress;
}
