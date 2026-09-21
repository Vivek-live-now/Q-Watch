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

    String getTimeStr(); // HH:MM or HH:MM AM/PM based on settings
    String getSecondsStr(); // SS
    String getDateStr(); // DD MMM YYYY

    static void onSntpSync(struct timeval *tv);

private:
    bool time_set;
    struct tm timeinfo;
    NtpSyncStatus sync_status;
    uint32_t sync_start_time;
    uint32_t last_sync_time;
    String current_tz;
};

extern Clock qclock;

#endif
