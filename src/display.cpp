#include "display.h"
#include "hw_config.h"
#include "wifi_portal.h"
#include "clock.h"
#include "weather.h"
#include "config.h"
#include "ui_core.h"
#include "sensors.h"
#include <SPI.h>
#include "battery.h"
#include "led_manager.h"
#include "sound_manager.h"

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
            case UIState::APP_ALTIMETER: drawAppAltimeter(); break;
case UIState::APP_BATTERY: drawAppBattery(); break;
            case UIState::APP_LED: drawAppLED(); break;
            case UIState::APP_AUDIO: drawAppAudio(); break;
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

void DisplayManager::drawTopStatusBar() {
    oled.setFont(u8g2_font_4x6_tr);

    // Time
    String t = qclock.getTimeStr();
    oled.drawStr(56, 6, t.c_str());

    // WiFi
    if (WiFi.status() == WL_CONNECTED) {
        oled.drawStr(100, 6, "W");
    } else {
        oled.drawStr(100, 6, "-");
    }

    // Battery
    int pct = battery.readPercentage();
    String bStr = String(pct) + "%";
    oled.drawStr(110, 6, bStr.c_str());
    // Draw battery tiny outline
    oled.drawFrame(1, 1, 10, 6);
    oled.drawBox(11, 2, 2, 4);
    oled.drawBox(2, 2, (pct * 8) / 100, 4);

    oled.drawLine(0, 8, 128, 8);
}
void DisplayManager::drawFooter(const char* status) {
    oled.drawLine(0, 54, 128, 54);
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 62, status);
}

void DisplayManager::drawPortalScreen() {
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 30, "LINK: Q-Watch-Setup");
    oled.drawStr(10, 45, "ADDR: 192.168.4.1");
}

void DisplayManager::drawAppHome() {
    drawTopStatusBar();
    for (int i=20; i<108; i+=4) oled.drawPixel(i, 35);

    oled.setFont(u8g2_font_logisoso24_tn);
    String timeStr = qclock.getTimeStr();
    int w_time = oled.getStrWidth(timeStr.c_str());
    oled.drawStr((128-w_time)/2, 42, timeStr.c_str());
}
void DisplayManager::drawAppClock() {
    oled.setFont(u8g2_font_logisoso28_tn);
    String timeStr = qclock.getTimeStr();
    int w_time = oled.getStrWidth(timeStr.c_str());
    oled.drawStr((128-w_time)/2, 45, timeStr.c_str());

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(110, 62, "[  ]");
    oled.drawStr(113, 62, qclock.getSecondsStr().c_str());
}

void DisplayManager::drawAppWeather() {
    oled.setFont(u8g2_font_ncenB14_tr);
    oled.drawStr(10, 30, "27\260C");

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 46, "HUM: 62%  WND: 1.2M/S");
}


void DisplayManager::drawAppCompass() {
    CompassState s = ui.getCompassState();

    if (s == CompassState::PAGE_METRICS) {
        drawAppCompassMetrics();
        return;
    } else if (s == CompassState::PAGE_CAL_MENU) {
        drawAppCompassCalMenu();
        return;
    } else if (s == CompassState::CAL_SWEEP) {
        drawAppCompassCalSweep();
        return;
    } else if (s == CompassState::CAL_RESULT) {
        drawAppCompassCalResult();
        return;
    } else if (s == CompassState::CAL_TELEMETRY) {
        drawAppCompassTelemetry();
        return;
    } else if (s == CompassState::CAL_DECLINATION) {
        drawAppCompassDeclination();
        return;
    }

    OrientationData o = sensors.getOrientation();
    float heading = o.yaw;

    // Convert heading string
    String hdgStr = String((int)heading) + "°";

    // Base center for the compass dial
    int cx = 64;
    int cy = 60;
    int r = 50;

    // Draw rotating dial
    for (int i = 0; i < 360; i += 15) {
        float angle = (i - heading - 90) * PI / 180.0;
        int x1 = cx + (r * cos(angle));
        int y1 = cy + (r * sin(angle));

        // Only draw visible upper half
        if (y1 <= cy + 5) {
            int len = (i % 90 == 0) ? 6 : (i % 30 == 0 ? 4 : 2);
            int x2 = cx + ((r - len) * cos(angle));
            int y2 = cy + ((r - len) * sin(angle));
            oled.drawLine(x1, y1, x2, y2);

            // Draw cardinal labels
            if (i % 90 == 0) {
                const char* lbl = (i == 0) ? "N" : (i == 90) ? "E" : (i == 180) ? "S" : "W";
                int tx = cx + ((r - 12) * cos(angle));
                int ty = cy + ((r - 12) * sin(angle));
                oled.setFont(u8g2_font_5x7_tr);
                int sw = oled.getStrWidth(lbl);
                oled.drawStr(tx - sw/2, ty + 3, lbl);
            } else if (i % 30 == 0) {
                String lbl = String(i);
                int tx = cx + ((r - 12) * cos(angle));
                int ty = cy + ((r - 12) * sin(angle));
                oled.setFont(u8g2_font_4x6_tr);
                int sw = oled.getStrWidth(lbl.c_str());
                oled.drawStr(tx - sw/2, ty + 3, lbl.c_str());
            }
        }
    }

    // Fixed indicator triangle at top
    oled.drawTriangle(64, 4, 60, 12, 68, 12);

    // Large heading text
    oled.setFont(u8g2_font_ncenB12_tr);

    const char* dirStr = "N";
    if (heading >= 337.5 || heading < 22.5) dirStr = "N";
    else if (heading >= 22.5 && heading < 67.5) dirStr = "NE";
    else if (heading >= 67.5 && heading < 112.5) dirStr = "E";
    else if (heading >= 112.5 && heading < 157.5) dirStr = "SE";
    else if (heading >= 157.5 && heading < 202.5) dirStr = "S";
    else if (heading >= 202.5 && heading < 247.5) dirStr = "SW";
    else if (heading >= 247.5 && heading < 292.5) dirStr = "W";
    else if (heading >= 292.5 && heading < 337.5) dirStr = "NW";

    String fullHdg = hdgStr + " " + String(dirStr);
    int w = oled.getStrWidth(fullHdg.c_str());
    oled.drawStr(64 - w/2, 60, fullHdg.c_str());
}
void DisplayManager::drawAppCompassMetrics() {

    CalibratedSensorData data = sensors.getCalData();
    float magTotal = sqrt(data.mx*data.mx + data.my*data.my + data.mz*data.mz);

    // Update history array
    mag_history[mag_history_idx] = magTotal;
    mag_history_idx = (mag_history_idx + 1) % 64;

    oled.setFont(u8g2_font_4x6_tr);
    String xStr = "X:" + String(data.mx, 1);
    String yStr = "Y:" + String(data.my, 1);
    String zStr = "Z:" + String(data.mz, 1);

    oled.drawStr(10, 22, xStr.c_str());
    oled.drawStr(50, 22, yStr.c_str());
    oled.drawStr(90, 22, zStr.c_str());

    String totStr = "TOTAL: " + String(magTotal, 1) + "uT";
    oled.drawStr(10, 32, totStr.c_str());

    // Draw graph
    int graph_x = 10;
    int graph_y = 52;
    int graph_w = 108;
    int graph_h = 15;

    oled.drawFrame(graph_x, graph_y - graph_h, graph_w, graph_h + 1);

    float max_val = 1.0;
    for (int i = 0; i < 64; i++) {
        if (mag_history[i] > max_val) max_val = mag_history[i];
    }

    for (int i = 0; i < 63; i++) {
        int idx1 = (mag_history_idx + i) % 64;
        int idx2 = (mag_history_idx + i + 1) % 64;

        int x1 = graph_x + 1 + (i * (graph_w - 2) / 63);
        int x2 = graph_x + 1 + ((i + 1) * (graph_w - 2) / 63);

        int y1 = graph_y - (mag_history[idx1] / max_val * graph_h);
        int y2 = graph_y - (mag_history[idx2] / max_val * graph_h);

        // Clamp to frame
        if (y1 < graph_y - graph_h + 1) y1 = graph_y - graph_h + 1;
        if (y2 < graph_y - graph_h + 1) y2 = graph_y - graph_h + 1;

        oled.drawLine(x1, y1, x2, y2);
    }
}
void DisplayManager::drawAppHealth() {
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 28, "BPM: ---");
    oled.drawStr(10, 42, "O2 : --- %");
}


void DisplayManager::drawAppMotionSettings() {
    oled.setFont(u8g2_font_6x10_tr);

    int sel = ui.getCompassMenuSelection();
    int offset = ui.getCompassMenuOffset();

    int y_pos = 22;
    for (int i = offset; i < offset + 3 && i < UICore::MOTION_MENU_ITEM_COUNT; i++) {
        if (i == sel) {
            oled.drawBox(2, y_pos - 8, 118, 10);
            oled.setDrawColor(0);
        }

        String label = ui.motion_menu_items[i];
        if (i == 0) label += sensors.getImuSwapXY() ? " [ON]" : " [OFF]";
        else if (i == 1) label += sensors.getImuInvX() ? " [ON]" : " [OFF]";
        else if (i == 2) label += sensors.getImuInvY() ? " [ON]" : " [OFF]";
        else if (i == 3) label += sensors.getImuInvZ() ? " [ON]" : " [OFF]";

        oled.drawStr(4, y_pos, label.c_str());
        oled.setDrawColor(1);
        y_pos += 12;
    }

    int scroll_h = 30;
    int scroll_y = 15 + ((float)offset / (UICore::MOTION_MENU_ITEM_COUNT - 3)) * (scroll_h - 10);
    oled.drawFrame(123, 15, 3, 30);
    oled.drawBox(123, scroll_y, 3, 10);
}

void DisplayManager::drawAppMotion() {
    MotionState s = ui.getMotionState();
    if (s == MotionState::PAGE_LEVEL) {
        drawAppMotionLevel();
    } else if (s == MotionState::PAGE_DATA) {
        drawAppMotionData();
    } else if (s == MotionState::PAGE_SETTINGS) {
        drawAppMotionSettings();
    }
}
void DisplayManager::drawAppMotionLevel() {
    OrientationData o = sensors.getOrientation();

    // Smooth layout for 128x64
    // 1. Big Bullseye on the left (x=32, y=32, r=30)
    int cx = 32;
    int cy = 32;
    int r = 30;

    oled.drawCircle(cx, cy, r);
    oled.drawCircle(cx, cy, 6);
    oled.drawLine(cx - r, cy, cx + r, cy);
    oled.drawLine(cx, cy - r, cx, cy + r);

    // Scale for visual: 30 degrees = max edge
    float scale = 30.0f;
    float r_roll = o.roll; if (r_roll > scale) r_roll = scale; if (r_roll < -scale) r_roll = -scale;
    float r_pitch = o.pitch; if (r_pitch > scale) r_pitch = scale; if (r_pitch < -scale) r_pitch = -scale;

    int bx = cx + (r_roll / scale) * (r - 4);
    int by = cy + (r_pitch / scale) * (r - 4);

    // Constrain bullseye bubble
    float dx = bx - cx; float dy = by - cy;
    float dist = sqrt(dx*dx + dy*dy);
    if (dist > (r - 4)) {
        bx = cx + (dx / dist) * (r - 4);
        by = cy + (dy / dist) * (r - 4);
    }
    oled.drawDisc(bx, by, 4);

    // 2. Vertical Pitch Bar (Right edge)
    int vp_x = 112; int vp_y = 2; int vp_w = 12; int vp_h = 60;
    oled.drawFrame(vp_x, vp_y, vp_w, vp_h);
    oled.drawLine(vp_x, cy, vp_x + vp_w - 1, cy); // center line

    int vp_by = cy + (r_pitch / scale) * (vp_h/2 - 5);
    oled.drawBox(vp_x + 2, vp_by - 4, vp_w - 4, 8);

    // 3. Horizontal Roll Bar (Top right corner, above text?)
    // Actually the image has Horizontal Roll at bottom.
    // Let's put Horizontal Roll Bar next to bullseye.
    int hr_x = 68; int hr_y = 48; int hr_w = 40; int hr_h = 12;
    oled.drawFrame(hr_x, hr_y, hr_w, hr_h);
    oled.drawLine(hr_x + hr_w/2, hr_y, hr_x + hr_w/2, hr_y + hr_h - 1); // center line

    int hr_bx = (hr_x + hr_w/2) + (r_roll / scale) * (hr_w/2 - 5);
    oled.drawBox(hr_bx - 4, hr_y + 2, 8, hr_h - 4);

    // 4. Data readout
    oled.setFont(u8g2_font_5x7_tr);
    String pStr = "P: " + String((int)o.pitch) + "°";
    String rStr = "R: " + String((int)o.roll) + "°";
    oled.drawStr(70, 20, pStr.c_str());
    oled.drawStr(70, 32, rStr.c_str());
}

void DisplayManager::drawAppMotionData() {
    oled.setFont(u8g2_font_4x6_tr);

    OrientationData o = sensors.getOrientation();
    CalibratedSensorData cal = sensors.getCalData();

    oled.drawStr(2, 22, "TILT/HDG");
    oled.drawStr(30, 22, ("P:" + String(o.pitch, 1)).c_str());
    oled.drawStr(65, 22, ("R:" + String(o.roll, 1)).c_str());
    oled.drawStr(100, 22, ("Y:" + String(o.yaw, 1)).c_str());

    oled.drawStr(2, 32, "ACC(g)");
    oled.drawStr(30, 32, ("X:" + String(cal.ax, 2)).c_str());
    oled.drawStr(65, 32, ("Y:" + String(cal.ay, 2)).c_str());
    oled.drawStr(100, 32, ("Z:" + String(cal.az, 2)).c_str());

    oled.drawStr(2, 42, "GYR(d/s)");
    oled.drawStr(30, 42, ("X:" + String(cal.gx, 1)).c_str());
    oled.drawStr(65, 42, ("Y:" + String(cal.gy, 1)).c_str());
    oled.drawStr(100, 42, ("Z:" + String(cal.gz, 1)).c_str());
}

void DisplayManager::drawAppIR() {
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 28, "EMITTER: ARMED");
    oled.drawStr(10, 42, "SENSOR : STANDBY");
}


void DisplayManager::drawAppBattery() {
    int pct = battery.readPercentage();
    float vol = battery.readVoltage();

    // Draw big battery graphic (x=34, y=10, w=60, h=25)
    oled.drawFrame(34, 10, 56, 25);
    oled.drawBox(90, 16, 4, 13); // terminal

    int fill_w = (pct * 52) / 100;
    if (fill_w > 0) {
        oled.drawBox(36, 12, fill_w, 21);
    }

    // Draw Text
    oled.setFont(u8g2_font_ncenB12_tr);
    String pctStr = String(pct) + "%";
    int w1 = oled.getStrWidth(pctStr.c_str());
    oled.drawStr(64 - w1/2, 50, pctStr.c_str());

    oled.setFont(u8g2_font_6x10_tr);
    String volStr = String(vol, 2) + "V";
    int w2 = oled.getStrWidth(volStr.c_str());
    oled.drawStr(64 - w2/2, 62, volStr.c_str());
}

void DisplayManager::drawAppLED() {
    drawTopStatusBar();
    oled.setFont(u8g2_font_6x10_tr);

    int sel = ui.getLedMenuSelection();
    int offset = ui.getLedMenuOffset();

    int y_pos = 22;
    for (int i = offset; i < offset + 3 && i < UICore::LED_MENU_ITEM_COUNT; i++) {
        if (i == sel) {
            oled.drawBox(2, y_pos - 8, 118, 10);
            oled.setDrawColor(0);
        }

        String label = ui.led_menu_items[i];
        if (i == 0) label += ledManager.isMasterSwitchOn() ? " [ON]" : " [OFF]";
        else if (i == 1) {
            LedMode m = ledManager.getMode();
            if (m == LedMode::OFF) label += " [OFF]";
            else if (m == LedMode::SOLID) label += " [SOLID]";
            else if (m == LedMode::BREATHING) label += " [BREATH]";
            else if (m == LedMode::RAINBOW) label += " [RNBW]";
            else if (m == LedMode::COMPASS_SYNC) label += " [SYNC]";
            else label += " [SYS]";
        }
        else if (i == 2) label += " [" + String(ledManager.getBrightness()) + "]";

        oled.drawStr(4, y_pos, label.c_str());
        oled.setDrawColor(1);
        y_pos += 12;
    }

    int scroll_h = 30;
    int scroll_y = 15 + ((float)offset / (UICore::LED_MENU_ITEM_COUNT - 3)) * (scroll_h - 10);
    oled.drawFrame(123, 15, 3, 30);
    oled.drawBox(123, scroll_y, 3, 10);
}



void DisplayManager::drawAppAltimeter() {
    oled.clearBuffer();
    drawTopStatusBar();

    EnvironmentData env = sensors.getEnvData();

    // Main Altitude Display
    oled.setFont(u8g2_font_logisoso16_tr);
    char buf[32];
    sprintf(buf, "%.1f m", env.altitude);
    int w = oled.getStrWidth(buf);
    oled.drawStr(64 - w/2, 34, buf);

    // Pressure & Temp
    oled.setFont(u8g2_font_4x6_tr);
    sprintf(buf, "PRESS: %.1f hPa", env.pressure);
    oled.drawStr(2, 48, buf);

    sprintf(buf, "TEMP: %.1f C", env.temperature);
    oled.drawStr(2, 56, buf);

    // Hint
    oled.drawStr(70, 56, "[ANY] ZERO");
}



void DisplayManager::drawAppAudio() {
    drawTopStatusBar();
    oled.setFont(u8g2_font_6x10_tr);

    int sel = ui.getAudioMenuSelection();
    int offset = ui.getAudioMenuOffset();

    int y_pos = 22;
    for (int i = offset; i < offset + 3 && i < UICore::AUDIO_MENU_ITEM_COUNT; i++) {
        if (i == sel) {
            oled.drawBox(2, y_pos - 8, 118, 10);
            oled.setDrawColor(0);
        }

        String label = ui.audio_menu_items[i];
        if (i == 0) label += soundManager.isMasterSwitchOn() ? " [ON]" : " [OFF]";
        else if (i == 1) {
            SoundStyle s = soundManager.getStyle();
            if (s == SoundStyle::SILENT) label += " [SILENT]";
            else if (s == SoundStyle::MODERN) label += " [MODERN]";
            else if (s == SoundStyle::TACTICAL) label += " [TACTICAL]";
            else if (s == SoundStyle::RETRO) label += " [RETRO]";
        }

        oled.drawStr(4, y_pos, label.c_str());
        oled.setDrawColor(1);
        y_pos += 12;
    }
}

void DisplayManager::drawAppAbout() {
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 25, "ID: 007 Q-WATCH");
    oled.drawStr(10, 35, "CORE: ESP32-S3 S-MINI");
    oled.drawStr(10, 45, "VER: FIRST LIGHT v0.1");
}

void DisplayManager::drawMenu(const char* title, const char** items, int item_count) {
    oled.setFont(u8g2_font_5x7_tr);
oled.drawStr(2, 7, title);
oled.drawLine(0, 9, 128, 9);
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
    oled.setFont(u8g2_font_5x7_tr);
oled.drawStr(2, 7, title);
oled.drawLine(0, 9, 128, 9);
    int val = ui.getEditValue();
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 26, "ADJUST PARAMETER:");

    oled.drawFrame(10, 34, 104, 10);
    if (val > 0) {
        for(int i = 0; i < val; i++) {
            oled.drawBox(12 + (i*10), 36, 8, 6);
        }
    }
}


void DisplayManager::drawAppCompassCalMenu() {
    oled.setFont(u8g2_font_6x10_tr);
    int sel = ui.getCompassMenuSelection();
    int offset = ui.getCompassMenuOffset();
    int y_pos = 22;

    for (int i = offset; i < offset + 3 && i < UICore::COMPASS_MENU_ITEM_COUNT; i++) {
        if (i == sel) {
            oled.drawBox(2, y_pos - 8, 118, 10);
            oled.setDrawColor(0);

            // Dynamic value appending
            String label = String(ui.compass_menu_items[i]);
            if (i == 1) { // Orient
                int o = sensors.getMagCalibration().orientation_mode;
                label += o == 0 ? " [YF]" : (o == 1 ? " [XF]" : (o == 2 ? " [YB]" : " [XB]"));
            } else if (i == 2) {
                label += sensors.getMagCalibration().invert_z ? " [ON]" : " [OFF]";
            }

            oled.drawStr(4, y_pos, label.c_str());
            oled.setDrawColor(1);
        } else {
            String label = String(ui.compass_menu_items[i]);
            oled.drawStr(4, y_pos, label.c_str());
        }
        y_pos += 12;
    }

    int scroll_h = 30;
    int scroll_y = 15 + ((float)offset / (UICore::COMPASS_MENU_ITEM_COUNT - 3)) * (scroll_h - 10);
    oled.drawFrame(123, 15, 3, 30);
    oled.drawBox(123, scroll_y, 3, 10);
}

void DisplayManager::drawAppCompassCalSweep() {
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 25, "ROTATE FIGURE 8");
    oled.drawStr(10, 35, "ALL AXES");

    oled.drawFrame(10, 45, 108, 6);
    int p = sensors.getCalProgress();
    oled.drawBox(10, 45, (p * 108) / 100, 6);
}

void DisplayManager::drawAppCompassCalResult() {
    oled.setFont(u8g2_font_5x7_tr);

    MagCalResult res = sensors.getCalResult();

    oled.drawStr(10, 22, "Coverage:");
    oled.drawStr(60, 22, res.coverage_ok ? "GOOD" : "POOR");

    oled.drawStr(10, 32, "Field:");
    oled.drawStr(60, 32, res.field_ok ? "GOOD" : "ERR");

    oled.drawStr(10, 42, "Overall:");
    oled.drawStr(60, 42, res.is_good ? "OK" : "BAD");
}

void DisplayManager::drawAppCompassTelemetry() {
    oled.setFont(u8g2_font_4x6_tr);

    RawSensorData raw = sensors.getRawData();
    CalibratedSensorData cal = sensors.getCalData();

    oled.drawStr(2, 22, "RAW");
    oled.drawStr(30, 22, ("X:" + String(raw.mx)).c_str());
    oled.drawStr(65, 22, ("Y:" + String(raw.my)).c_str());
    oled.drawStr(100, 22, ("Z:" + String(raw.mz)).c_str());

    oled.drawStr(2, 32, "CAL");
    oled.drawStr(30, 32, ("X:" + String(cal.mx, 1)).c_str());
    oled.drawStr(65, 32, ("Y:" + String(cal.my, 1)).c_str());
    oled.drawStr(100, 32, ("Z:" + String(cal.mz, 1)).c_str());

    oled.drawStr(2, 42, "HDG:");
    oled.drawStr(30, 42, String(sensors.getOrientation().yaw, 1).c_str());

    MagCalibration mcal = sensors.getMagCalibration();
    oled.drawStr(65, 42, ("DEC:" + String(mcal.declination, 1)).c_str());
}

void DisplayManager::drawAppCompassDeclination() {

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(10, 26, "ADJUST OFFSET:");

    oled.setFont(u8g2_font_ncenB12_tr);
    String dStr = String(sensors.getMagCalibration().declination, 1) + "\260";
    int w = oled.getStrWidth(dStr.c_str());
    oled.drawStr(64 - w/2, 44, dStr.c_str());
}
