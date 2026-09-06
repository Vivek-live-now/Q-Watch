#ifndef DISPLAY_H
#define DISPLAY_H

#if defined(ARDUINO)
#include <Arduino.h>
#include <U8g2lib.h>
#endif

class DisplayManager {
public:
    void begin();
    void update();

private:
    void drawAppHome();
    void drawAppClock();
    void drawAppWeather();
    void drawAppCompass();
    void drawAppHealth();
    void drawAppMotion();
    void drawAppIR();
    void drawAppGames();
    void drawAppBattery();
    void drawAppAbout();

    void drawPortalScreen();
    void drawMenu(const char* title, const char** items, int item_count);
    void drawValueEdit(const char* title);

    // Helpers
    void drawHeader(const char* title);
    void drawFooter(const char* status);
    void drawTacticalOverlay();
};

extern DisplayManager displayManager;

#if defined(ARDUINO)
extern U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI oled;
#endif

#endif
