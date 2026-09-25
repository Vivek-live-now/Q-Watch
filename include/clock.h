#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>
#include <time.h>

enum class NtpSyncStatus {
    IDLE,
    SYNCING,
    SUCCESS,
    FAILED
};

class Clock {
public:
    void begin(const String& timezone);
    void loop();

    void syncNtp();
    void setTimezone(const String& posix_tz);
    void setTimezoneIdx(int idx);

    bool isTimeSet();
    NtpSyncStatus getSyncStatus() const { return sync_status; }
    uint32_t getLastSyncTime() const { return last_sync_time; }
    uint32_t getEpoch() const { time_t now; time(&now); return (uint32_t)now; }
    void setEpoch(uint32_t epoch);

    String getTimeStr(); // HH:MM or HH:MM AM/PM based on settings
    void getTimeStr(char* buf, size_t maxLen) const;
    String getSecondsStr(); // SS
    void getSecondsStr(char* buf, size_t maxLen) const;
    int getSecond() const;
    int getHour() const { return time_set ? timeinfo.tm_hour : 0; }
    int getMinute() const { return time_set ? timeinfo.tm_min : 0; }
    int getDayOfWeek() const { return time_set ? timeinfo.tm_wday : 0; }
    int getDay() const { return time_set ? timeinfo.tm_mday : 1; }
    int getMonth() const { return time_set ? (timeinfo.tm_mon + 1) : 1; }
    int getYear() const { return time_set ? (timeinfo.tm_year + 1900) : 2026; }
    String getDayOfWeekStr() const;
    const char* getDayOfWeekCStr() const;
    String getMonthStr() const;
    const char* getMonthCStr() const;
    String getDateStr(); // DD MMM YYYY
    void getDateStr(char* buf, size_t maxLen) const;

    static void onSntpSync(struct timeval *tv);

private:
    bool time_set;
    struct tm timeinfo;
    NtpSyncStatus sync_status;
    uint32_t sync_start_time;
    uint32_t last_sync_time;
    uint32_t last_time_poll;
    String current_tz;
};

extern Clock qclock;

#endif
