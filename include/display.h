#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "simd_accel.h"
#include "gdma_manager.h"

class DisplayManager {
public:
    DisplayManager() : mag_history_idx(0), display_dimmed(false), transition_active(false), transition_type(SIMD_TRANSITION_NONE), transition_start_time(0), transition_duration_ms(120), gdma_enabled(true) {
        for(int i=0; i<64; i++) mag_history[i] = 0;
    }
public:
    void begin();
    void update();
    void applyDisplaySettings();
    void setPowerSave(bool enable);
    void setDim(bool dim);
    bool isDim() const { return display_dimmed; }
    const uint8_t* getBufferPtr() const;
    size_t getBufferSize() const { return 1024; }
    void invertBuffer();
    void xorBuffer(const uint8_t* mask);
    void andBuffer(const uint8_t* mask);
    void orBuffer(const uint8_t* mask);

    // Hardware GDMA Asynchronous Display Refresh
    bool isGdmaEnabled() const { return gdma_enabled; }
    void setGdmaEnabled(bool enable) { gdma_enabled = enable; }
    void flushAsync();
    const GdmaTransferStats& getGdmaStats() const;

    // Vector Screen Transitions (128-bit SIMD Animation Engine)
    void startTransition(simd_transition_t type, uint16_t duration_ms = 120);
    void captureTransitionSnapshot();
    bool isTransitionActive() const { return transition_active; }

private:
    void drawAppHome();
    void drawHomeDigital();
    void drawHomeAnalog();
    void drawHomeRetro();
    void drawHomeMission();

    void drawAppClock();
    void drawClockMenu();
    void drawClockFaceSelect();
    void drawClockFaceWidgets();
    void drawClockStopwatch();
    void drawClockTimer();
    void drawClockAlarms();
    void drawClockAlarmEdit();
    void drawClockWorldClock();
    void drawClockPedometer();

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
    void drawAppCompassOrientation3D();

    float mag_history[64];
    int mag_history_idx;
    void drawAppHealth();
    void drawHealthPage1Live();
    void drawHealthPage2History();
    void drawAppMotion();
    void drawAppMotionMenu();
    void drawAppMotionLevel();
    void drawAppMotionData();
    void drawAppMotionSettings();
    void drawAppMotionOrientation3D();
    void drawAppAirMouse();
    void drawAppMouseSettings();
    void drawAppGestureAi();
    void drawAppGestureAiHud();
    void drawAppGestureAiSettings();
    void drawAppWireframe3D();
    void drawAppIR();
    void drawAppBme();
    void drawBmePage1Pressure();
    void drawBmePage2Humidity();
    void drawBmePage3Temperature();
    void drawBmePage4Altitude();
    void drawBmePage5Info();
    void drawAppBattery();
    void drawBatteryPageHud();
    void drawBatteryPageProfiles();
    void drawBatteryPageSleep();
    void drawBatteryPagePeripherals();
    void drawBatteryPageUlp();
    void drawAppLED();
    void drawAppAudio();
    void drawPolySynth();
    void drawAppAbout();
    void drawFileManager();
    void drawAppApps();
    void drawAppAnimList();
    void drawAppAnimPlayer();
    void drawAppWireless();
    void drawAppMochi();
    void drawAppVibration();
    void drawReconMainMenu();
    void drawBleList();
    void drawBleRadar();
    void drawWifiSpectrum();
    void drawDeauthDetect();
    void drawPacketMonitor();
    void drawStorageInfo();

    // Settings Screens Rendering
    void drawAppSettings();
    void drawSettingsMenuWithValues(const char* title, const char** items, const String* values, int item_count, int selection, int offset);
    void drawSettingsMenuWithValues(const char* title, const char** items, const char* const* values, int item_count, int selection, int offset);
    void drawWifiDetailsScreen();
    void drawWifiScanScreen();
    void drawFileServerDetailsScreen();
    void drawTimeSyncStatusScreen();
    void drawResetConfirm();

    // Keyboard Screen Rendering
    void drawKeyboardScreen();

    void drawPortalScreen();
    void drawScrollBar(int offset, int item_count);
    void drawStandardMenu(const char* title, const char** items, int item_count, int selection, int offset);
    void drawStandardMenu(const char* title, const char** items, int item_count, int selection, int offset, const String* values);
    void drawStandardMenu(const char* title, const char** items, int item_count, int selection, int offset, const char* const* values);
    void drawMenu(const char* title, const char** items, int item_count);
    void drawValueEdit(const char* title);

    // Helpers
    void drawTopStatusBar();
    void drawTacticalOverlay();
    void drawToastOverlay();

    bool display_dimmed;
    bool gdma_enabled;
    bool transition_active;
    simd_transition_t transition_type;
    uint32_t transition_start_time;
    uint16_t transition_duration_ms;
    uint8_t transition_old_buf[1024] __attribute__((aligned(16)));
    uint8_t transition_mask_buf[1024] __attribute__((aligned(16)));
};

extern DisplayManager displayManager;
extern U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI oled;

#endif
