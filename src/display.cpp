#include "display.h"
#include "hw_config.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include <SPI.h>

// Define the global oled object here
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
        drawWatchFace();
    }

    oled.sendBuffer();
}

void DisplayManager::drawPortalScreen() {
    oled.setFont(u8g2_font_ncenB08_tr);
    oled.drawFrame(0, 0, 128, 64);

    oled.drawStr(10, 20, "AP: Q-Watch-Setup");
    oled.drawStr(10, 40, "IP: 192.168.4.1");
}

void DisplayManager::drawWatchFace() {
    // 1. Time
    oled.setFont(u8g2_font_logisoso24_tn); // Large font for HH:MM
    String timeStr = qclock.getTimeStr();
    int w_time = oled.getStrWidth(timeStr.c_str());
    oled.drawStr(5, 28, timeStr.c_str());

    // Seconds
    oled.setFont(u8g2_font_ncenB10_tr);
    oled.drawStr(5 + w_time + 5, 28, qclock.getSecondsStr().c_str());

    // 2. Date
    oled.setFont(u8g2_font_ncenB08_tr);
    String dateStr = qclock.getDateStr();
    oled.drawStr(5, 42, dateStr.c_str());

    // 3. Weather
    const WeatherData& wd = weather.getData();
    if (wd.valid) {
        String condStr = weather.mapWmoCodeToCondition(wd.weather_code);
        String weatherStr1 = String(wd.temperature, 1) + "C " + condStr;
        String weatherStr2 = "H:" + String(wd.humidity) + "% W:" + String(wd.wind_speed, 1) + "km/h";

        oled.setFont(u8g2_font_5x7_tr);
        oled.drawStr(5, 53, weatherStr1.c_str());
        oled.drawStr(5, 62, weatherStr2.c_str());
    } else {
        oled.setFont(u8g2_font_ncenB08_tr);
        oled.drawStr(5, 58, "Weather: Syncing...");
    }
}
