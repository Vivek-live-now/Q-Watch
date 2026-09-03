#include "display.h"
#include "hw_config.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include "config.h"
#include "ui_core.h"
#include <SPI.h>

U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI oled(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);

DisplayManager displayManager;

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
            case UIState::APP_ABOUT: drawAppAbout(); break;

            case UIState::MAIN_MENU:
                drawMenu("MAIN MENU", ui.main_menu_items, UICore::MAIN_MENU_ITEM_COUNT);
                break;
            case UIState::APP_SETTINGS:
                drawMenu("SETTINGS", ui.settings_menu_items, UICore::SETTINGS_MENU_ITEM_COUNT);
                break;
            case UIState::VALUE_EDIT:
                drawValueEdit("EDIT VALUE");
                break;
            case UIState::SLEEPING:
                break;
        }
    }

    oled.sendBuffer();
}

void DisplayManager::drawHeader(const char* title) {
    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(0, 10, title);
    oled.drawLine(0, 12, 128, 12);
}

void DisplayManager::drawFooter(const char* status) {
    oled.drawLine(0, 52, 128, 52);
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(0, 62, status);
}

void DisplayManager::drawPortalScreen() {
    drawHeader("Q-WATCH SETUP");
    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(5, 28, "AP: Q-Watch-Setup");
    oled.drawStr(5, 42, "IP: 192.168.4.1");
    drawFooter("Status: WAITING");
}

void DisplayManager::drawAppHome() {
    drawHeader("Q-WATCH");

    // Time
    oled.setFont(u8g2_font_logisoso24_tn);
    String timeStr = qclock.getTimeStr();
    int w_time = oled.getStrWidth(timeStr.c_str());
    oled.drawStr((128-w_time)/2, 42, timeStr.c_str());

    drawFooter(WiFi.status() == WL_CONNECTED ? "Status: ONLINE" : "Status: OFFLINE");
}

void DisplayManager::drawAppClock() {
    drawHeader("CLOCK");

    oled.setFont(u8g2_font_logisoso28_tn);
    String timeStr = qclock.getTimeStr();
    int w_time = oled.getStrWidth(timeStr.c_str());
    oled.drawStr((128-w_time)/2, 45, timeStr.c_str());

    // Tiny seconds bottom right
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(115, 62, qclock.getSecondsStr().c_str());
}

void DisplayManager::drawAppWeather() {
    drawHeader("WEATHER");

    oled.setFont(u8g2_font_ncenB14_tr);
    oled.drawStr(5, 30, "27\260C"); // 27 deg C

    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(5, 46, "Humidity 62%");

    drawFooter("Status: DEMO");
}

void DisplayManager::drawAppCompass() {
    drawHeader("COMPASS");

    oled.setFont(u8g2_font_ncenB12_tr);
    oled.drawStr(5, 30, "Hdg: 247\260");

    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(5, 46, "Dir: WSW");

    drawFooter("Status: DEMO");
}

void DisplayManager::drawAppHealth() {
    drawHeader("HEALTH");

    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(5, 28, "Heart Rate: -- BPM");
    oled.drawStr(5, 42, "SpO2: -- %");

    drawFooter("Status: SENSOR OFFLINE");
}

void DisplayManager::drawAppMotion() {
    drawHeader("MOTION");

    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(5, 28, "Gyro: STANDBY");
    oled.drawStr(5, 42, "Level: 0\260");

    drawFooter("Status: DEMO");
}

void DisplayManager::drawAppIR() {
    drawHeader("IR REMOTE");

    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(5, 28, "IR TX: READY");
    oled.drawStr(5, 42, "IR RX: READY");

    drawFooter("Status: DEMO");
}

void DisplayManager::drawAppGames() {
    drawHeader("GAMES");

    oled.setFont(u8g2_font_ncenB10_tr);
    oled.drawStr(15, 35, "OG BOUNCE");

    drawFooter("Status: DEMO");
}

void DisplayManager::drawAppAbout() {
    drawHeader("ABOUT");

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(5, 25, "007 Q-WATCH");
    oled.drawStr(5, 35, "ESP32-S3 SuperMini");
    oled.drawStr(5, 45, "First Light v0.1");

    drawFooter("Build: 2026");
}

void DisplayManager::drawMenu(const char* title, const char** items, int item_count) {
    drawHeader(title);

    oled.setFont(u8g2_font_ncenB08_tr);
    int start_idx = ui.getMenuScrollOffset();
    int y_pos = 25;

    for (int i = start_idx; i < start_idx + 4 && i < item_count; i++) {
        if (i == ui.getMenuSelection()) {
            oled.drawStr(0, y_pos, ">");
        }
        oled.drawStr(10, y_pos, items[i]);
        y_pos += 10; // Tighter spacing for 128x64
    }

    if (item_count > 4) {
        int scroll_h = 40;
        int scroll_y = 15 + ((float)start_idx / (item_count - 4)) * (scroll_h - 10);
        oled.drawBox(125, scroll_y, 3, 10);
    }
}

void DisplayManager::drawValueEdit(const char* title) {
    drawHeader(title);

    int val = ui.getEditValue();

    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawStr(10, 26, "Use UP/DN to adjust");

    oled.drawFrame(10, 36, 104, 12);
    if (val > 0) {
        oled.drawBox(12, 38, val * 10, 8);
    }

    drawFooter("SEL:Save  L-SEL:Cancel");
}
