#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>

struct WeatherData {
    float temperature;
    float wind_speed;
    int humidity;
    int weather_code; // WMO code
    bool valid;
};

class Weather {
public:
    Weather();
    void loop();
    void forceUpdate();
    const WeatherData& getData() const;
    String mapWmoCodeToCondition(int code);

private:
    WeatherData data;
    uint32_t last_update_time;
    bool needs_update;

    void fetchWeather();
};

extern Weather weather;

#endif
