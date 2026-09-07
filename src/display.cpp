#include "display.h"
#include "hw_config.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include "config.h"
#include "ui_core.h"
#include "sensors.h"
#include "battery.h"
#include "led_controller.h"
#include <SPI.h>

U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI oled(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);

DisplayManager displayManager;

const uint8_t battery_icon[] U8X8_PROGMEM = {
  0x7e, 0xbd, 0x81, 0x81, 0x81, 0x81, 0xbd, 0x7e
};

void DisplayManager::begin() {
    SPI.begin(OLED_CLK, -1, OLED_MOSI, OLED_CS);
    oled.begin();
    oled.clearBuffer();
    oled.sendBuffer();
}

void DisplayManager::update() {
    oled.clearBuffer();

    if (wifiPortal.getState() == WifiState::PORTAL) {
        drawPortalScreen();
    } else {
        switch (ui.getState()) {
            case UIState::APP_HOME: drawAppHome(); break;
            case UIState::APP_CLOCK: drawAppClock(); break;
            case UIState::APP_WEATHER: drawAppWeather(); break;
            case UIState::APP_COMPASS: drawAppCompass(); break;
            case UIState::APP_HEALTH: drawAppHealth(); break;
            case UIState::APP_MOTION: drawAppMotion(); break;
            case UIState::APP_IR: drawAppIR(); break;
            case UIState::APP_GAMES: drawAppGames(); break;
            case UIState::APP_BATTERY: drawAppBattery(); break;
            case UIState::APP_LED: drawAppLED(); break;
            case UIState::APP_ABOUT: drawAppAbout(); break;

            case UIState::MAIN_MENU:
                drawMenu("SYS MENU", ui.main_menu_items, UICore::MAIN_MENU_ITEM_COUNT);
                break;
            case UIState::APP_SETTINGS:
                drawMenu("CFG/OPTS", ui.settings_menu_items, UICore::SETTINGS_MENU_ITEM_COUNT);
                break;
            case UIState::VALUE_EDIT:
                drawValueEdit("ADJUST");
                break;
            case UIState::SLEEPING:
                break;
        }

        drawTacticalOverlay();
    }

    oled.sendBuffer();
}

void DisplayManager::drawTacticalOverlay() {
    oled.drawLine(0, 0, 6, 0); oled.drawLine(0, 0, 0, 6);
    oled.drawLine(127, 0, 121, 0); oled.drawLine(127, 0, 127, 6);
    oled.drawLine(0, 63, 6, 63); oled.drawLine(0, 63, 0, 57);
    oled.drawLine(127, 63, 121, 63); oled.drawLine(127, 63, 127, 57);
}

void DisplayManager::drawHeader(const char* title) {
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 7, title);

    if (WiFi.status() == WL_CONNECTED) {
        oled.drawStr(100, 7, "W");
    } else {
        oled.drawStr(100, 7, "-");
    }

    oled.drawXBMP(116, 0, 8, 8, battery_icon);
    oled.drawLine(0, 9, 128, 9);
}

void DisplayManager::drawPortalScreen() {
    drawHeader("SYS/CFG");
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 30, "LINK: Q-Watch-Setup");
    oled.drawStr(10, 48, "ADDR: 192.168.4.1");
}

void DisplayManager::drawAppHome() {
    drawHeader("Q-WATCH OP");
    for (int i=20; i<108; i+=4) oled.drawPixel(i, 20);

    oled.setFont(u8g2_font_logisoso24_tn);
    String timeStr = qclock.getTimeStr();
    int w_time = oled.getStrWidth(timeStr.c_str());
    oled.drawStr((128-w_time)/2, 48, timeStr.c_str());
}

void DisplayManager::drawAppClock() {
    drawHeader("CHRONO");
    oled.setFont(u8g2_font_logisoso28_tn);
    String timeStr = qclock.getTimeStr();
    int w_time = oled.getStrWidth(timeStr.c_str());
    oled.drawStr((128-w_time)/2, 48, timeStr.c_str());

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(108, 60, qclock.getSecondsStr().c_str());
}

void DisplayManager::drawAppWeather() {
    drawHeader("ATMOS");
    oled.setFont(u8g2_font_ncenB14_tr);
    oled.drawStr(10, 30, "27\260C");

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 48, "HUM: 62%  WND: 1.2M/S");
}

void DisplayManager::drawAppCompass() {
    drawHeader("NAV/HDG");

    OrientationData o = sensors.getOrientation();
    String hdgStr = "HDG [" + String((int)o.yaw) + "\260 ]";

    oled.setFont(u8g2_font_ncenB12_tr);
    oled.drawStr(10, 30, hdgStr.c_str());

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 48, "TILT COMPENSATED");
}

void DisplayManager::drawAppHealth() {
    drawHeader("BIO/METRIC");
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 28, "BPM: ---");
    oled.drawStr(10, 44, "O2 : --- %");
}

void DisplayManager::drawAppMotion() {
    drawHeader("IMU/GYRO");

    OrientationData o = sensors.getOrientation();
    String pStr = "PITCH: " + String((int)o.pitch) + "\260";
    String rStr = "ROLL : " + String((int)o.roll) + "\260";

    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 28, pStr.c_str());
    oled.drawStr(10, 44, rStr.c_str());
}

void DisplayManager::drawAppIR() {
    drawHeader("OPT/INFRA");
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 28, "EMITTER: ARMED");
    oled.drawStr(10, 44, "SENSOR : STANDBY");
}

void DisplayManager::drawAppGames() {
    drawHeader("ARCHIVE");
    oled.setFont(u8g2_font_ncenB10_tr);
    oled.drawStr(15, 38, "OG_BOUNCE.EXE");
}

void DisplayManager::drawAppBattery() {
    drawHeader("PWR/CELL");
    float v = battery.readVoltage();
    int pct = battery.readPercentage();

    oled.setFont(u8g2_font_6x10_tr);
    String vStr = "VOLT: " + String(v, 2) + " V";
    String pctStr = "CAP : " + String(pct) + " %";
    oled.drawStr(10, 25, vStr.c_str());
    oled.drawStr(10, 36, pctStr.c_str());

    // Progress bar for battery percentage
    oled.drawFrame(10, 42, 108, 12);
    int fill_w = (pct * 104) / 100;
    if (fill_w > 104) fill_w = 104;
    if (fill_w > 0) {
        oled.drawBox(12, 44, fill_w, 8);
    }
}

void DisplayManager::drawAppLED() {
    drawHeader("LED MATRIX");
    oled.setFont(u8g2_font_5x7_tr);

    String pwrStr = "PWR: " + String(ledController.isEnabled() ? "ON" : "OFF");
    String brtStr = "BRT: " + String((ledController.getBrightness() * 100) / 255) + "%";

    oled.drawStr(10, 24, pwrStr.c_str());
    oled.drawStr(70, 24, brtStr.c_str());

    const char* presetNames[] = {"RED", "GREEN", "BLUE", "PURPLE", "AMBER", "WHITE", "RAINBOW", "CUSTOM"};
    int pIdx = (int)ledController.getPreset();
    if (pIdx < 0 || pIdx > 7) pIdx = 0;
    String modeStr = "PRESET: " + String(presetNames[pIdx]);
    oled.drawStr(10, 36, modeStr.c_str());

    RGBColor col = ledController.getCurrentColor();
    String rgbStr = "RGB: " + String(col.r) + "," + String(col.g) + "," + String(col.b);
    oled.drawStr(10, 48, rgbStr.c_str());

    oled.drawFrame(10, 52, 108, 8);
    int fill = (ledController.getBrightness() * 104) / 255;
    if (fill > 0 && ledController.isEnabled()) {
        oled.drawBox(12, 54, fill, 4);
    }
}

void DisplayManager::drawAppAbout() {
    drawHeader("SYS/INFO");
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 25, "ID: 007 Q-WATCH");
    oled.drawStr(10, 35, "CORE: ESP32-S3 S-MINI");
    oled.drawStr(10, 45, "VER: FIRST LIGHT v0.1");
}

void DisplayManager::drawMenu(const char* title, const char** items, int item_count) {
    drawHeader(title);
    oled.setFont(u8g2_font_6x10_tr);
    int start_idx = ui.getMenuScrollOffset();
    int y_pos = 24;

    for (int i = start_idx; i < start_idx + 3 && i < item_count; i++) {
        if (i == ui.getMenuSelection()) {
            oled.drawBox(2, y_pos - 8, 118, 11);
            oled.setDrawColor(0);
            oled.drawStr(4, y_pos, items[i]);
            oled.setDrawColor(1);
        } else {
            oled.drawStr(4, y_pos, items[i]);
        }
        y_pos += 13;
    }

    if (item_count > 3) {
        int scroll_h = 36;
        int scroll_y = 15 + ((float)start_idx / (item_count - 3)) * (scroll_h - 10);
        oled.drawFrame(123, 15, 3, 36);
        oled.drawBox(123, scroll_y, 3, 10);
    }
}

void DisplayManager::drawValueEdit(const char* title) {
    drawHeader(title);
    int val = ui.getEditValue();
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 26, "ADJUST PARAMETER:");

    oled.drawFrame(10, 38, 104, 12);
    if (val > 0) {
        for(int i = 0; i < val; i++) {
            oled.drawBox(12 + (i*10), 40, 8, 8);
        }
    }
}
