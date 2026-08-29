#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>

struct WeatherData {
    float temperature;
    float feels_like;
    float wind_speed;
    int humidity;
    String condition;
    bool valid;
};

class Weather {
public:
    Weather();
    void loop();
    void forceUpdate();
    const WeatherData& getData() const;
    uint32_t getLastUpdateTime() const;
    bool isUpdateInProgress() const;

private:
    WeatherData data;
    uint32_t last_update_time;
    bool needs_update;
    bool in_progress;

    // Non-blocking Task Handle
    TaskHandle_t weatherTaskHandle;
    static void weatherTask(void *pvParameters);
};

extern Weather weather;

#endif
