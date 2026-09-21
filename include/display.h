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
    void applyDisplaySettings();
    void setPowerSave(bool enable);

private:
    void drawAppHome();
    void drawAppClock();
    void drawAppWeather();
    void drawWeatherPage1Local();
    void drawWeatherPage2Owm();
    void drawWeatherPage3TempGraph();
    void drawWeatherPage4PressGraph();
    void drawWeatherPage5HumGraph();
    void drawWeatherPage6Settings();
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
    void drawHealthLive();
    void drawHealthToday();
    void drawHealthSettings();
    void drawAppMotion();
    void drawAppMotionLevel();
    void drawAppMotionData();
    void drawAppMotionSettings();
    void drawAppIR();
    void drawAppBme();
    void drawBmePage1Pressure();
    void drawBmePage2Humidity();
    void drawBmePage3Temperature();
    void drawBmePage4Altitude();
    void drawBmePage5Info();
    void drawAppBattery();
    void drawAppLED();
    void drawAppAudio();
    void drawAppAbout();
    void drawFileManager();
    void drawStorageInfo();

    // Settings Screens Rendering
    void drawAppSettings();
    void drawSettingsMenuWithValues(const char* title, const char** items, const String* values, int item_count, int selection, int offset);
    void drawWifiDetailsScreen();
    void drawWifiScanScreen();
    void drawFileServerDetailsScreen();
    void drawTimeSyncStatusScreen();
    void drawResetConfirm();

    // Keyboard Screen Rendering
    void drawKeyboardScreen();

    void drawPortalScreen();
    void drawMenu(const char* title, const char** items, int item_count);
    void drawValueEdit(const char* title);

    // Helpers
    void drawTopStatusBar();
    void drawFooter(const char* status);
    void drawTacticalOverlay();
    void drawToastOverlay();
};

extern DisplayManager displayManager;
extern U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI oled;

#endif
