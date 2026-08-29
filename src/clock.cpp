#include "clock.h"
#include "esp_sntp.h"

Clock qclock;

void sntp_sync_time_cb(struct timeval *tv) {
    Serial.println("NTP Time Synced!");
}

void Clock::begin(const String& timezone) {
    time_set = false;

    sntp_set_time_sync_notification_cb(sntp_sync_time_cb);
    configTzTime(timezone.c_str(), "pool.ntp.org", "time.nist.gov");
}

void Clock::loop() {
    if (getLocalTime(&timeinfo, 0)) {
        time_set = true;
    }
}

bool Clock::isTimeSet() {
    return time_set;
}

String Clock::getTimeStr() {
    if (!time_set) return "--:--";
    char buffer[6];
    strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
    return String(buffer);
}

String Clock::getSecondsStr() {
    if (!time_set) return "--";
    char buffer[3];
    strftime(buffer, sizeof(buffer), "%S", &timeinfo);
    return String(buffer);
}

String Clock::getDateStr() {
    if (!time_set) return "Syncing...";
    char buffer[12];
    strftime(buffer, sizeof(buffer), "%d %b %Y", &timeinfo);
    return String(buffer);
}
