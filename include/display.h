#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>

class DisplayManager {
public:
    DisplayManager() : mag_history_idx(0) {
        for(int i=0; i<64; i++) mag_history[i] = 0;
    }
public:
    void begin();
    void update();

private:
    void drawAppHome();
    void drawAppClock();
    void drawAppWeather();
    void drawAppCompass();
    void drawAppCompassMetrics();

    void drawAppCompassCalMenu();
    void drawAppCompassCalSweep();
    void drawAppCompassCalResult();
    void drawAppCompassTelemetry();
    void drawAppCompassDeclination();

    float mag_history[64];
    int mag_history_idx;
    void drawAppHealth();
    void drawAppMotion();
    void drawAppMotionLevel();
    void drawAppMotionData();
    void drawAppMotionSettings();
    void drawAppIR();
    void drawAppDebug();
    void drawAppBattery();
    void drawAppLED();
    void drawAppAudio();
    void drawAppAbout();

    void drawPortalScreen();
    void drawMenu(const char* title, const char** items, int item_count);
    void drawValueEdit(const char* title);

    // Helpers
    void drawTopStatusBar();
    void drawFooter(const char* status);
    void drawTacticalOverlay();
};

extern DisplayManager displayManager;
extern U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI oled;

#endif
