#ifndef TIMEKEEPING_H
#define TIMEKEEPING_H

#include <Arduino.h>

struct StopwatchLap {
    uint32_t total_ms;
    uint32_t lap_ms;
};

class Stopwatch {
public:
    Stopwatch();
    void start();
    void pause();
    void resume();
    void reset();
    void lap();

    bool isRunning() const { return running; }
    bool isPaused() const { return paused; }
    uint32_t getElapsedMs() const;
    int getLapCount() const { return lap_count; }
    StopwatchLap getLap(int idx) const;

    static String formatMs(uint32_t ms);

private:
    bool running;
    bool paused;
    uint32_t start_time;
    uint32_t accumulated_ms;
    uint32_t last_lap_total_ms;
    static const int MAX_LAPS = 8;
    StopwatchLap laps[MAX_LAPS];
    int lap_count;
};

class CountdownTimer {
public:
    CountdownTimer();
    void setDuration(uint32_t sec);
    void start();
    void pause();
    void resume();
    void reset();
    void loop();

    bool isRunning() const { return running; }
    bool isPaused() const { return paused; }
    bool isExpired() const { return expired; }
    void clearExpired() { expired = false; }
    uint32_t getDuration() const { return duration_sec; }
    uint32_t getRemaining() const { return remaining_sec; }

    static String formatSec(uint32_t sec);

private:
    uint32_t duration_sec;
    uint32_t remaining_sec;
    uint32_t last_tick_ms;
    bool running;
    bool paused;
    bool expired;
};

struct AlarmEntry {
    uint8_t hour;     // 0..23
    uint8_t minute;   // 0..59
    bool enabled;
    bool triggered_today;
};

class AlarmManager {
public:
    AlarmManager();
    void loop();
    int getAlarmCount() const { return 3; }
    AlarmEntry& getAlarm(int idx) { return alarms[idx % 3]; }
    const AlarmEntry& getAlarm(int idx) const { return alarms[idx % 3]; }
    void toggleAlarm(int idx);
    void setAlarmTime(int idx, uint8_t h, uint8_t m);
    void snooze(int idx);
    void dismiss(int idx);
    bool isRinging() const { return ringing_idx >= 0; }
    int getRingingIdx() const { return ringing_idx; }
    void stopRinging() { ringing_idx = -1; }

    void load();
    void save();

private:
    AlarmEntry alarms[3];
    int ringing_idx;
    uint32_t last_ring_beep;
    int last_checked_min;
};

class Pedometer {
public:
    Pedometer();
    void update(float ax, float ay, float az);
    uint32_t getSteps() const { return step_count; }
    float getDistanceKm() const { return step_count * 0.00075f; }
    uint32_t getCaloriesKcal() const { return (uint32_t)(step_count * 0.04f); }
    void reset() { step_count = 0; }
    void setSteps(uint32_t s) { step_count = s; }

private:
    uint32_t step_count;
    float last_mag;
    uint32_t last_step_time;
    bool armed;
};

class TimekeepingManager {
public:
    TimekeepingManager();
    void begin();
    void loop();

    Stopwatch stopwatch;
    CountdownTimer timer;
    AlarmManager alarmManager;
    Pedometer pedometer;

    void checkHourlyChime();

    String getWorldTimeStr(int tz_idx) const;
    String getWorldCityName(int tz_idx) const;
    String getWorldDateOffsetStr(int tz_idx) const;

private:
    int last_chime_hour;
};

extern TimekeepingManager timekeeping;

#endif
