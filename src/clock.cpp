#include "wifi_portal.h"
#include "clock.h"
#include "esp_sntp.h"
#include "settings_data.h"
#include "config.h"
#include <WiFi.h>

Clock qclock;

// POSIX strings matching TIMEZONE_OPTIONS in settings_data.h
static const char* const POSIX_TZ_MAP[] = {
    "IST-5:30",                  // Asia/Kolkata
    "UTC0",                      // UTC
    "GMT0BST,M3.5.0/1,M10.5.0",  // Europe/London
    "CET-1CEST,M3.5.0,M10.5.0/3",// Europe/Berlin
    "EST5EDT,M3.2.0,M11.1.0",    // America/New_York
    "CST6CDT,M3.2.0,M11.1.0",    // America/Chicago
    "MST7MDT,M3.2.0,M11.1.0",    // America/Denver
    "PST8PDT,M3.2.0,M11.1.0",    // America/Los_Angeles
    "GST-4",                     // Asia/Dubai
    "SGT-8",                     // Asia/Singapore
    "JST-9",                     // Asia/Tokyo
    "AEST-10AEDT,M10.1.0,M4.1.0" // Australia/Sydney
};

void Clock::onSntpSync(struct timeval *tv) {
    Serial.println("NTP Time Synced Callback Received!");
    qclock.sync_status = NtpSyncStatus::SUCCESS;
    qclock.last_sync_time = millis();
    qclock.time_set = true;

    // Check Wi-Fi Auto-Off mode 1: After Sync
    if (settingsManager.get().wifi_auto_off_idx == 1) {
        Serial.println("Auto-off Wi-Fi after successful NTP sync...");
        wifiPortal.disableWifi();
    }
}

void Clock::begin(const String& timezone) {
    time_set = false;
    sync_status = NtpSyncStatus::IDLE;
    sync_start_time = 0;
    last_sync_time = 0;
    last_time_poll = 0;
    current_tz = timezone;

    sntp_set_time_sync_notification_cb(Clock::onSntpSync);
    configTzTime(current_tz.c_str(), "pool.ntp.org", "time.nist.gov", "time.google.com");
}

void Clock::setTimezone(const String& posix_tz) {
    current_tz = posix_tz;
    configTzTime(current_tz.c_str(), "pool.ntp.org", "time.nist.gov", "time.google.com");
}

void Clock::setTimezoneIdx(int idx) {
    if (idx >= 0 && idx < TIMEZONE_OPTION_COUNT) {
        current_tz = POSIX_TZ_MAP[idx];
        configTzTime(current_tz.c_str(), "pool.ntp.org", "time.nist.gov", "time.google.com");
    }
}

void Clock::setEpoch(uint32_t epoch) {
    struct timeval tv = { (time_t)epoch, 0 };
    settimeofday(&tv, nullptr);
    time_set = true;
    last_sync_time = millis();
    sync_status = NtpSyncStatus::SUCCESS;
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

static uint32_t last_auto_sync_check = 0;

void Clock::loop() {
    uint32_t now = millis();
    if (now - last_time_poll >= 50) {
        last_time_poll = now;
        if (getLocalTime(&timeinfo, 0)) {
            time_set = true;
        }
    }

    if (sync_status == NtpSyncStatus::SYNCING) {
        if (millis() - sync_start_time > 10000) {
            Serial.println("SNTP Sync timed out.");
            sync_status = NtpSyncStatus::FAILED;
        }
    }

    // Periodic 24-hour Auto Sync if enabled and Wi-Fi connected
    if (settingsManager.get().auto_sync && WiFi.status() == WL_CONNECTED) {
        if (millis() - last_auto_sync_check > 60000) {
            last_auto_sync_check = millis();
            if (last_sync_time == 0 || (millis() - last_sync_time > 86400000UL)) {
                syncNtp();
            }
        }
    }
}

bool Clock::isTimeSet() {
    return time_set;
}

void Clock::getTimeStr(char* buf, size_t maxLen) const {
    if (!buf || maxLen == 0) return;
    if (!time_set) {
        snprintf(buf, maxLen, "--:--");
        return;
    }
    bool format_24 = settingsManager.get().format_24hr;
    if (format_24) {
        strftime(buf, maxLen, "%H:%M", &timeinfo);
    } else {
        strftime(buf, maxLen, "%I:%M%p", &timeinfo);
    }
}

String Clock::getTimeStr() {
    char buffer[12];
    getTimeStr(buffer, sizeof(buffer));
    return String(buffer);
}

void Clock::getSecondsStr(char* buf, size_t maxLen) const {
    if (!buf || maxLen == 0) return;
    if (!time_set) {
        snprintf(buf, maxLen, "--");
        return;
    }
    strftime(buf, maxLen, "%S", &timeinfo);
}

String Clock::getSecondsStr() {
    char buffer[4];
    getSecondsStr(buffer, sizeof(buffer));
    return String(buffer);
}

int Clock::getSecond() const {
    return time_set ? timeinfo.tm_sec : -1;
}

const char* Clock::getDayOfWeekCStr() const {
    if (!time_set) return "---";
    static const char* const days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    int d = getDayOfWeek();
    if (d >= 0 && d < 7) return days[d];
    return "---";
}

String Clock::getDayOfWeekStr() const {
    return String(getDayOfWeekCStr());
}

const char* Clock::getMonthCStr() const {
    if (!time_set) return "---";
    static const char* const months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    int m = getMonth() - 1;
    if (m >= 0 && m < 12) return months[m];
    return "---";
}

String Clock::getMonthStr() const {
    return String(getMonthCStr());
}

void Clock::getDateStr(char* buf, size_t maxLen) const {
    if (!buf || maxLen == 0) return;
    if (!time_set) {
        snprintf(buf, maxLen, "Syncing...");
        return;
    }
    strftime(buf, maxLen, "%d %b %Y", &timeinfo);
}

String Clock::getDateStr() {
    char buffer[16];
    getDateStr(buffer, sizeof(buffer));
    return String(buffer);
}
