#include "timekeeping.h"
#include "sound_manager.h"
#include "settings_data.h"
#include "clock.h"
#include "sensors.h"
#include "file_manager.h"

TimekeepingManager timekeeping;

// =================== STOPWATCH ===================

Stopwatch::Stopwatch() :
    running(false),
    paused(false),
    start_time(0),
    accumulated_ms(0),
    last_lap_total_ms(0),
    lap_count(0)
{
    for (int i = 0; i < MAX_LAPS; i++) {
        laps[i] = {0, 0};
    }
}

void Stopwatch::start() {
    running = true;
    paused = false;
    start_time = millis();
    accumulated_ms = 0;
    last_lap_total_ms = 0;
    lap_count = 0;
}

void Stopwatch::pause() {
    if (running && !paused) {
        accumulated_ms += (millis() - start_time);
        paused = true;
    }
}

void Stopwatch::resume() {
    if (running && paused) {
        start_time = millis();
        paused = false;
    }
}

void Stopwatch::reset() {
    running = false;
    paused = false;
    start_time = 0;
    accumulated_ms = 0;
    last_lap_total_ms = 0;
    lap_count = 0;
}

void Stopwatch::lap() {
    if (!running) return;
    uint32_t current_total = getElapsedMs();
    uint32_t lap_duration = current_total - last_lap_total_ms;
    last_lap_total_ms = current_total;

    if (lap_count < MAX_LAPS) {
        laps[lap_count++] = {current_total, lap_duration};
    } else {
        // Shift laps left to keep the most recent
        for (int i = 0; i < MAX_LAPS - 1; i++) {
            laps[i] = laps[i + 1];
        }
        laps[MAX_LAPS - 1] = {current_total, lap_duration};
    }
}

uint32_t Stopwatch::getElapsedMs() const {
    if (!running) return 0;
    if (paused) return accumulated_ms;
    return accumulated_ms + (millis() - start_time);
}

StopwatchLap Stopwatch::getLap(int idx) const {
    if (idx >= 0 && idx < lap_count) {
        return laps[idx];
    }
    return {0, 0};
}

String Stopwatch::formatMs(uint32_t ms) {
    uint32_t total_sec = ms / 1000;
    uint32_t minutes = total_sec / 60;
    uint32_t seconds = total_sec % 60;
    uint32_t hundredths = (ms % 1000) / 10;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u.%02u", (unsigned)minutes, (unsigned)seconds, (unsigned)hundredths);
    return String(buf);
}

// =================== COUNTDOWN TIMER ===================

CountdownTimer::CountdownTimer() :
    duration_sec(300), // Default: 5 min
    remaining_sec(300),
    last_tick_ms(0),
    running(false),
    paused(false),
    expired(false)
{}

void CountdownTimer::setDuration(uint32_t sec) {
    duration_sec = (sec > 0) ? sec : 60;
    remaining_sec = duration_sec;
    running = false;
    paused = false;
    expired = false;
}

void CountdownTimer::start() {
    remaining_sec = duration_sec;
    running = true;
    paused = false;
    expired = false;
    last_tick_ms = millis();
}

void CountdownTimer::pause() {
    if (running && !paused) {
        paused = true;
    }
}

void CountdownTimer::resume() {
    if (running && paused) {
        paused = false;
        last_tick_ms = millis();
    }
}

void CountdownTimer::reset() {
    running = false;
    paused = false;
    expired = false;
    remaining_sec = duration_sec;
}

void CountdownTimer::loop() {
    if (!running || paused) return;

    uint32_t now = millis();
    if (now - last_tick_ms >= 1000) {
        last_tick_ms = now;
        if (remaining_sec > 0) {
            remaining_sec--;
            if (remaining_sec == 0) {
                running = false;
                expired = true;
                soundManager.playAlert();
            }
        }
    }
}

String CountdownTimer::formatSec(uint32_t sec) {
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;

    char buf[16];
    if (h > 0) {
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
    } else {
        snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)m, (unsigned)s);
    }
    return String(buf);
}

// =================== ALARM MANAGER ===================

AlarmManager::AlarmManager() :
    ringing_idx(-1),
    last_ring_beep(0),
    last_checked_min(-1)
{
    alarms[0] = {7, 0, false, false};   // Alarm 1: 07:00
    alarms[1] = {12, 30, false, false}; // Alarm 2: 12:30
    alarms[2] = {18, 0, false, false};  // Alarm 3: 18:00
}

void AlarmManager::toggleAlarm(int idx) {
    if (idx >= 0 && idx < 3) {
        alarms[idx].enabled = !alarms[idx].enabled;
        alarms[idx].triggered_today = false;
        save();
    }
}

void AlarmManager::setAlarmTime(int idx, uint8_t h, uint8_t m) {
    if (idx >= 0 && idx < 3) {
        alarms[idx].hour = h % 24;
        alarms[idx].minute = m % 60;
        alarms[idx].triggered_today = false;
        save();
    }
}

void AlarmManager::snooze(int idx) {
    if (idx >= 0 && idx < 3) {
        alarms[idx].minute = (alarms[idx].minute + 5) % 60;
        alarms[idx].triggered_today = false;
    }
    ringing_idx = -1;
}

void AlarmManager::dismiss(int idx) {
    (void)idx;
    ringing_idx = -1;
}

void AlarmManager::loop() {
    int cur_hour = qclock.getHour();
    int cur_min = qclock.getMinute();

    if (cur_min != last_checked_min) {
        last_checked_min = cur_min;
        for (int i = 0; i < 3; i++) {
            if (alarms[i].enabled && alarms[i].hour == cur_hour && alarms[i].minute == cur_min) {
                if (!alarms[i].triggered_today) {
                    alarms[i].triggered_today = true;
                    ringing_idx = i;
                    soundManager.playAlert();
                    last_ring_beep = millis();
                }
            } else if (alarms[i].minute != cur_min) {
                alarms[i].triggered_today = false;
            }
        }
    }

    if (ringing_idx >= 0) {
        // Repeated alert tone every 2.5 seconds while ringing
        if (millis() - last_ring_beep >= 2500) {
            last_ring_beep = millis();
            soundManager.playAlert();
        }
    }
}

void AlarmManager::load() {
    if (!fileManager.exists("/config/alarms")) return;
    String content = fileManager.read("/config/alarms");
    if (content.length() == 0) return;

    int pos = 0;
    while (pos < content.length()) {
        int next_nl = content.indexOf('\n', pos);
        if (next_nl == -1) next_nl = content.length();
        String line = content.substring(pos, next_nl);
        line.trim();
        pos = next_nl + 1;

        if (line.startsWith("alm0=")) {
            int h, m, en;
            if (sscanf(line.c_str() + 5, "%d,%d,%d", &h, &m, &en) == 3) {
                alarms[0].hour = h; alarms[0].minute = m; alarms[0].enabled = (en == 1);
            }
        } else if (line.startsWith("alm1=")) {
            int h, m, en;
            if (sscanf(line.c_str() + 5, "%d,%d,%d", &h, &m, &en) == 3) {
                alarms[1].hour = h; alarms[1].minute = m; alarms[1].enabled = (en == 1);
            }
        } else if (line.startsWith("alm2=")) {
            int h, m, en;
            if (sscanf(line.c_str() + 5, "%d,%d,%d", &h, &m, &en) == 3) {
                alarms[2].hour = h; alarms[2].minute = m; alarms[2].enabled = (en == 1);
            }
        }
    }
}

void AlarmManager::save() {
    String out = "";
    for (int i = 0; i < 3; i++) {
        out += "alm" + String(i) + "=" + String(alarms[i].hour) + "," + String(alarms[i].minute) + "," + String(alarms[i].enabled ? 1 : 0) + "\n";
    }
    fileManager.write("/config/alarms", out);
}

// =================== PEDOMETER ===================

Pedometer::Pedometer() :
    step_count(0),
    last_mag(1.0f),
    last_step_time(0),
    armed(false)
{}

void Pedometer::update(float ax, float ay, float az) {
    float mag = sqrtf(ax * ax + ay * ay + az * az);

    if (mag > 1.25f && !armed) {
        armed = true;
    } else if (armed && mag < 0.95f) {
        uint32_t now = millis();
        if (now - last_step_time >= 240) { // Max ~4 steps/sec
            step_count++;
            last_step_time = now;
        }
        armed = false;
    }
    last_mag = mag;
}

// =================== TIMEKEEPING MANAGER ===================

TimekeepingManager::TimekeepingManager() :
    last_chime_hour(-1)
{}

void TimekeepingManager::begin() {
    alarmManager.load();
}

void TimekeepingManager::loop() {
    timer.loop();
    alarmManager.loop();

    // Update pedometer from IMU
    CalibratedSensorData cal = sensors.getCalData();
    pedometer.update(cal.ax, cal.ay, cal.az);

    checkHourlyChime();
}

void TimekeepingManager::checkHourlyChime() {
    if (!settingsManager.get().hourly_chime_enabled) return;

    int cur_min = qclock.getMinute();
    int cur_sec = qclock.getSecond();
    int cur_hour = qclock.getHour();

    if (cur_min == 0 && cur_sec == 0) {
        if (cur_hour != last_chime_hour) {
            last_chime_hour = cur_hour;
            soundManager.playTone(3200, 30);
            delay(40);
            soundManager.playTone(3200, 50);
        }
    }
}

// UTC offsets in quarter-hours (e.g. 5.5h = 22 quarters)
static const int TZ_QUARTERS[] = {
    22,   // Asia/Kolkata (UTC +5:30)
    0,    // UTC (UTC +0:00)
    0,    // London (UTC +0:00)
    4,    // Berlin (UTC +1:00)
    -20,  // New York (UTC -5:00)
    -24,  // Chicago (UTC -6:00)
    -28,  // Denver (UTC -7:00)
    -32,  // Los Angeles (UTC -8:00)
    16,   // Dubai (UTC +4:00)
    32,   // Singapore (UTC +8:00)
    36,   // Tokyo (UTC +9:00)
    40    // Sydney (UTC +10:00)
};

static const char* const CITY_NAMES[] = {
    "KOLKATA", "UTC", "LONDON", "BERLIN", "NEW YORK",
    "CHICAGO", "DENVER", "LOS ANGELES", "DUBAI", "SINGAPORE", "TOKYO", "SYDNEY"
};

String TimekeepingManager::getWorldCityName(int tz_idx) const {
    if (tz_idx >= 0 && tz_idx < 12) {
        return String(CITY_NAMES[tz_idx]);
    }
    return "UTC";
}

String TimekeepingManager::getWorldTimeStr(int tz_idx) const {
    if (tz_idx < 0 || tz_idx >= 12) tz_idx = 1;

    uint32_t epoch = qclock.getEpoch();
    if (epoch == 0) return "--:--";

    // Target timezone offset in seconds
    int32_t offset_sec = (int32_t)TZ_QUARTERS[tz_idx] * 900;
    time_t target_epoch = (time_t)(epoch + offset_sec);

    struct tm target_tm;
    gmtime_r(&target_epoch, &target_tm);

    char buf[12];
    bool f24 = settingsManager.get().format_24hr;
    if (f24) {
        snprintf(buf, sizeof(buf), "%02d:%02d", target_tm.tm_hour, target_tm.tm_min);
    } else {
        int h = target_tm.tm_hour % 12;
        if (h == 0) h = 12;
        const char* ampm = (target_tm.tm_hour >= 12) ? "PM" : "AM";
        snprintf(buf, sizeof(buf), "%d:%02d%s", h, target_tm.tm_min, ampm);
    }
    return String(buf);
}

String TimekeepingManager::getWorldDateOffsetStr(int tz_idx) const {
    if (tz_idx < 0 || tz_idx >= 12) tz_idx = 1;

    uint32_t epoch = qclock.getEpoch();
    if (epoch == 0) return "";

    int32_t target_offset = (int32_t)TZ_QUARTERS[tz_idx] * 900;
    int local_idx = settingsManager.get().timezone_idx;
    int32_t local_offset = (local_idx >= 0 && local_idx < 12) ? ((int32_t)TZ_QUARTERS[local_idx] * 900) : 0;

    time_t local_t = (time_t)(epoch + local_offset);
    time_t target_t = (time_t)(epoch + target_offset);

    struct tm local_tm, target_tm;
    gmtime_r(&local_t, &local_tm);
    gmtime_r(&target_t, &target_tm);

    if (target_tm.tm_yday > local_tm.tm_yday || (target_tm.tm_year > local_tm.tm_year)) {
        return "(+1 DAY)";
    } else if (target_tm.tm_yday < local_tm.tm_yday || (target_tm.tm_year < local_tm.tm_year)) {
        return "(-1 DAY)";
    }
    return "(SAME DAY)";
}
