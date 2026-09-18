#include "clock.h"
#include "esp_sntp.h"
#include "settings_data.h"
#include "config.h"
#include <WiFi.h>

Clock qclock;

// POSIX strings matching TIMEZONE_OPTIONS in settings_data.h
// 0: UTC, 1: IST (+5:30), 2: EST (-5), 3: PST (-8)
static const char* const POSIX_TZ_MAP[] = {
    "UTC0",
    "IST-5:30",
    "EST5EDT,M3.2.0,M11.1.0",
    "PST8PDT,M3.2.0,M11.1.0"
};

void Clock::onSntpSync(struct timeval *tv) {
    Serial.println("NTP Time Synced Callback Received!");
    qclock.sync_status = NtpSyncStatus::SUCCESS;
    qclock.last_sync_time = millis();
    qclock.time_set = true;
}

void Clock::begin(const String& timezone) {
    time_set = false;
    sync_status = NtpSyncStatus::IDLE;
    sync_start_time = 0;
    last_sync_time = 0;
    current_tz = timezone;

    sntp_set_time_sync_notification_cb(Clock::onSntpSync);
    configTzTime(current_tz.c_str(), "pool.ntp.org", "time.nist.gov", "time.google.com");
}

void Clock::setTimezone(const String& posix_tz) {
    current_tz = posix_tz;
    configTzTime(current_tz.c_str(), "pool.ntp.org", "time.nist.gov", "time.google.com");
}

void Clock::syncNtp() {
    if (WiFi.status() != WL_CONNECTED) {
        sync_status = NtpSyncStatus::FAILED;
        return;
    }

    Serial.println("Initiating SNTP time sync...");
    sync_status = NtpSyncStatus::SYNCING;
    sync_start_time = millis();

    int tz_idx = settingsManager.get().timezone_idx;
    if (tz_idx >= 0 && tz_idx < TIMEZONE_OPTION_COUNT) {
        current_tz = POSIX_TZ_MAP[tz_idx];
    }
    configTzTime(current_tz.c_str(), "pool.ntp.org", "time.nist.gov", "time.google.com");
}

void Clock::loop() {
    if (getLocalTime(&timeinfo, 0)) {
        time_set = true;
    }

    if (sync_status == NtpSyncStatus::SYNCING) {
        if (millis() - sync_start_time > 10000) {
            Serial.println("SNTP Sync timed out.");
            sync_status = NtpSyncStatus::FAILED;
        }
    }
}

bool Clock::isTimeSet() {
    return time_set;
}

String Clock::getTimeStr() {
    if (!time_set) return "--:--";
    char buffer[12];
    bool format_24 = settingsManager.get().format_24hr;
    if (format_24) {
        strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
    } else {
        strftime(buffer, sizeof(buffer), "%I:%M%p", &timeinfo);
    }
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
