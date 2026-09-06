#include "display.h"
#include "hw_config.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include "config.h"
#include "ui_core.h"
#include "sensors.h"
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

void DisplayManager::drawFooter(const char* status) {
    oled.drawLine(0, 54, 128, 54);
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 62, status);
}

void DisplayManager::drawPortalScreen() {
    drawHeader("SYS/CFG");
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 30, "LINK: Q-Watch-Setup");
    oled.drawStr(10, 45, "ADDR: 192.168.4.1");
    drawFooter("STS: AWAITING UPLINK");
}

void DisplayManager::drawAppHome() {
    drawHeader("Q-WATCH OP");
    for (int i=20; i<108; i+=4) oled.drawPixel(i, 35);

    oled.setFont(u8g2_font_logisoso24_tn);
    String timeStr = qclock.getTimeStr();
    int w_time = oled.getStrWidth(timeStr.c_str());
    oled.drawStr((128-w_time)/2, 42, timeStr.c_str());

    drawFooter(WiFi.status() == WL_CONNECTED ? "LINK: ESTABLISHED" : "LINK: SEVERED");
}

void DisplayManager::drawAppClock() {
    drawHeader("CHRONO");
    oled.setFont(u8g2_font_logisoso28_tn);
    String timeStr = qclock.getTimeStr();
    int w_time = oled.getStrWidth(timeStr.c_str());
    oled.drawStr((128-w_time)/2, 45, timeStr.c_str());

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(110, 62, "[  ]");
    oled.drawStr(113, 62, qclock.getSecondsStr().c_str());
}

void DisplayManager::drawAppWeather() {
    drawHeader("ATMOS");
    oled.setFont(u8g2_font_ncenB14_tr);
    oled.drawStr(10, 30, "27\260C");

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 46, "HUM: 62%  WND: 1.2M/S");

    drawFooter("STS: OFFLINE DEMO");
}

void DisplayManager::drawAppCompass() {
    drawHeader("NAV/HDG");

    OrientationData o = sensors.getOrientation();
    String hdgStr = "HDG [" + String((int)o.yaw) + "\260 ]";

    oled.setFont(u8g2_font_ncenB12_tr);
    oled.drawStr(10, 30, hdgStr.c_str());

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 46, "TILT COMPENSATED");

    drawFooter(sensors.isMagOk() ? "STS: ACTIVE" : "STS: MAG FAIL");
}

void DisplayManager::drawAppHealth() {
    drawHeader("BIO/METRIC");
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 28, "BPM: ---");
    oled.drawStr(10, 42, "O2 : --- %");
    drawFooter("STS: NO PULSE");
}

void DisplayManager::drawAppMotion() {
    drawHeader("IMU/GYRO");

    OrientationData o = sensors.getOrientation();
    String pStr = "PITCH: " + String((int)o.pitch) + "\260";
    String rStr = "ROLL : " + String((int)o.roll) + "\260";

    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 28, pStr.c_str());
    oled.drawStr(10, 42, rStr.c_str());

    drawFooter(sensors.isMpuOk() ? "STS: TRACKING" : "STS: MPU FAIL");
}

void DisplayManager::drawAppIR() {
    drawHeader("OPT/INFRA");
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 28, "EMITTER: ARMED");
    oled.drawStr(10, 42, "SENSOR : STANDBY");
    drawFooter("STS: NO TARGET");
}

void DisplayManager::drawAppGames() {
    drawHeader("ARCHIVE");
    oled.setFont(u8g2_font_ncenB10_tr);
    oled.drawStr(15, 35, "OG_BOUNCE.EXE");
    drawFooter("STS: ENCRYPTED");
}

void DisplayManager::drawAppAbout() {
    drawHeader("SYS/INFO");
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 25, "ID: 007 Q-WATCH");
    oled.drawStr(10, 35, "CORE: ESP32-S3 S-MINI");
    oled.drawStr(10, 45, "VER: FIRST LIGHT v0.1");
    drawFooter("AUTH: MI6 CLEARED");
}

void DisplayManager::drawMenu(const char* title, const char** items, int item_count) {
    drawHeader(title);
    oled.setFont(u8g2_font_6x10_tr);
    int start_idx = ui.getMenuScrollOffset();
    int y_pos = 22;

    for (int i = start_idx; i < start_idx + 3 && i < item_count; i++) {
        if (i == ui.getMenuSelection()) {
            oled.drawBox(2, y_pos - 8, 118, 10);
            oled.setDrawColor(0);
            oled.drawStr(4, y_pos, items[i]);
            oled.setDrawColor(1);
        } else {
            oled.drawStr(4, y_pos, items[i]);
        }
        y_pos += 12;
    }

    if (item_count > 3) {
        int scroll_h = 30;
        int scroll_y = 15 + ((float)start_idx / (item_count - 3)) * (scroll_h - 10);
        oled.drawFrame(123, 15, 3, 30);
        oled.drawBox(123, scroll_y, 3, 10);
    }
}

void DisplayManager::drawValueEdit(const char* title) {
    drawHeader(title);
    int val = ui.getEditValue();
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 26, "ADJUST PARAMETER:");

    oled.drawFrame(10, 34, 104, 10);
    if (val > 0) {
        for(int i = 0; i < val; i++) {
            oled.drawBox(12 + (i*10), 36, 8, 6);
        }
    }
    drawFooter("SEL:SET  L-SEL:ABORT");
}
