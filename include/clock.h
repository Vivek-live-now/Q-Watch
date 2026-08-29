#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>
#include <time.h>

class Clock {
public:
    void begin(const String& timezone);
    void loop();

    bool isTimeSet();
    String getTimeStr(); // HH:MM
    String getSecondsStr(); // SS
    String getDateStr(); // DD MMM YYYY

private:
    bool time_set;
    struct tm timeinfo;
};

extern Clock qclock;

#endif
