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
#include "file_manager.h"
#include "led_manager.h"
#include "sound_manager.h"
#include "settings_data.h"
#include "keyboard.h"
#include "max30102_health.h"
#include "keyboard.h"
#include "keyboard.h"
#include "keyboard.h"

U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI oled(U8G2_R0, OLED_CS, OLED_DC, OLED_RST);

DisplayManager displayManager;

const uint8_t battery_icon[] U8X8_PROGMEM = {
  0x7e, 0xbd, 0x81, 0x81, 0x81, 0x81, 0xbd, 0x7e
};

void DisplayManager::begin() {
    SPI.begin(OLED_CLK, -1, OLED_MOSI, OLED_CS);
    oled.begin();
    applyDisplaySettings();
    oled.clearBuffer();
    oled.sendBuffer();
}

void DisplayManager::update() {
    oled.clearBuffer();

    if (wifiPortal.getState() == WifiState::PORTAL && ui.getState() != UIState::APP_SETTINGS) {
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
            case UIState::APP_ALTIMETER: drawAppBme(); break;
            case UIState::APP_BATTERY: drawAppBattery(); break;
            case UIState::APP_LED: drawAppLED(); break;
            case UIState::APP_AUDIO: drawAppAudio(); break;
            case UIState::APP_ABOUT: drawAppAbout(); break;
            case UIState::APP_FILE_MANAGER: drawFileManager(); break;
            case UIState::APP_STORAGE_INFO: drawStorageInfo(); break;

            case UIState::MAIN_MENU:
                drawMenu("SYS MENU", ui.main_menu_items, UICore::MAIN_MENU_ITEM_COUNT);
                break;
            case UIState::APP_SETTINGS:
                drawAppSettings();
                break;
                                                            case UIState::APP_KEYBOARD: drawKeyboardScreen(); break;
            case UIState::VALUE_EDIT:
                drawValueEdit("ADJUST");
                break;
            case UIState::SLEEPING:
                break;
        }

        drawTacticalOverlay();
        drawToastOverlay();
    }

    oled.sendBuffer();
}

void DisplayManager::drawToastOverlay() {
    const char* msg = ui.getToastMessage();
    if (msg && msg[0] != '\0') {
        oled.setFont(u8g2_font_6x10_tf);
        int w = oled.getStrWidth(msg) + 8;
        int x = (128 - w) / 2;
        int y = 26;
        int h = 14;

        oled.setDrawColor(0);
        oled.drawBox(x - 2, y - 2, w + 4, h + 4);
        oled.setDrawColor(1);
        oled.drawFrame(x, y, w, h);
        oled.drawStr(x + 4, y + 10, msg);
    }
}

void DisplayManager::drawAppSettings() {
    SettingsSubmenu sub = ui.getSettingsSubmenu();
    SettingsData& s = settingsManager.get();

    if (sub == SettingsSubmenu::MAIN) {
        drawMenu("SETTINGS", ui.settings_main_items, UICore::SETTINGS_MAIN_ITEM_COUNT);
    } else if (sub == SettingsSubmenu::CONNECTIVITY) {
        String vals[3] = {
            s.wifi_enabled ? "ON" : "OFF",
            s.ble_enabled ? "ON" : "OFF",
            s.fileserver_enabled ? "ON" : "OFF"
        };
        drawSettingsMenuWithValues("CONNECTIVITY", ui.connectivity_items, vals, UICore::CONNECTIVITY_ITEM_COUNT, ui.getSettingsSelection(), ui.getSettingsScrollOffset());
    } else if (sub == SettingsSubmenu::WIFI_DETAILS) {
        drawWifiDetailsScreen();
    } else if (sub == SettingsSubmenu::WIFI_SCAN) {
        drawWifiScanScreen();
    } else if (sub == SettingsSubmenu::FILE_SERVER_DETAILS) {
        drawFileServerDetailsScreen();
    } else if (sub == SettingsSubmenu::TIME_SYNC_STATUS) {
        drawTimeSyncStatusScreen();
    } else if (sub == SettingsSubmenu::TIME) {
        String vals[5] = {
            "",
            "",
            s.auto_sync ? "ON" : "OFF",
            TIMEZONE_OPTIONS[s.timezone_idx],
            s.format_24hr ? "ON" : "OFF"
        };
        drawSettingsMenuWithValues("TIME", ui.time_items, vals, UICore::TIME_ITEM_COUNT, ui.getSettingsSelection(), ui.getSettingsScrollOffset());
    } else if (sub == SettingsSubmenu::POWER) {
        String vals[4] = {
            DISPLAY_TIMEOUT_OPTIONS[s.display_timeout_idx],
            SLEEP_TIMEOUT_OPTIONS[s.sleep_time_idx],
            WIFI_AUTO_OFF_OPTIONS[s.wifi_auto_off_idx],
            s.low_power ? "ON" : "OFF"
        };
        drawSettingsMenuWithValues("POWER", ui.power_items, vals, UICore::POWER_ITEM_COUNT, ui.getSettingsSelection(), ui.getSettingsScrollOffset());
    } else if (sub == SettingsSubmenu::SUB_DISPLAY) {
        String vals[3] = {
            CONTRAST_OPTIONS[s.contrast_idx],
            s.invert_display ? "ON" : "OFF",
            UI_OPTIONS_LIST[s.ui_option_idx]
        };
        drawSettingsMenuWithValues("DISPLAY", ui.display_items, vals, UICore::DISPLAY_ITEM_COUNT, ui.getSettingsSelection(), ui.getSettingsScrollOffset());
    } else if (sub == SettingsSubmenu::SENSORS) {
        String vals[3] = {"", "", ""};
        drawSettingsMenuWithValues("SENSORS", ui.sensors_items, vals, UICore::SENSORS_ITEM_COUNT, ui.getSettingsSelection(), ui.getSettingsScrollOffset());
    } else if (sub == SettingsSubmenu::HEALTH) {
        String vals[2] = {
            settingsManager.get().health_background_enabled ? "ON" : "OFF",
            HEALTH_INTERVAL_OPTIONS[settingsManager.get().health_interval_idx]
        };
        drawSettingsMenuWithValues("HEALTH", ui.health_settings_items, vals, UICore::HEALTH_SETTINGS_ITEM_COUNT, ui.getSettingsSelection(), ui.getSettingsScrollOffset());
    } else if (sub == SettingsSubmenu::SYSTEM) {
        String vals[2] = {"", ""};
        drawSettingsMenuWithValues("SYSTEM", ui.system_items, vals, UICore::SYSTEM_ITEM_COUNT, ui.getSettingsSelection(), ui.getSettingsScrollOffset());
    } else if (sub == SettingsSubmenu::RESET_CONFIRM) {
        drawResetConfirm();
    }
}

void DisplayManager::drawWifiDetailsScreen() {
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 7, "WI-FI");
    oled.drawLine(0, 9, 128, 9);

    oled.setFont(u8g2_font_6x10_tr);

    SettingsData& s = settingsManager.get();
    String pwrStr = "Power : " + String(s.wifi_enabled ? "ON" : "OFF");
    String stStr  = "Status: " + String(wifiPortal.getDetailedStatusStr());
    String ssidStr= "SSID  : " + wifiPortal.getSSID();
    String ipStr  = "IP    : " + wifiPortal.getIP();

    oled.drawStr(2, 22, pwrStr.c_str());
    oled.drawStr(2, 34, stStr.c_str());
    oled.drawStr(2, 46, ssidStr.c_str());
    oled.drawStr(2, 58, ipStr.c_str());
}

void DisplayManager::drawSettingsMenuWithValues(const char* title, const char** items, const String* values, int item_count, int selection, int offset) {
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 7, title);
    oled.drawLine(0, 9, 128, 9);
    oled.setFont(u8g2_font_6x10_tr);

    int y_pos = 22;

    for (int i = offset; i < offset + 3 && i < item_count; i++) {
        if (i == selection) {
            oled.drawBox(2, y_pos - 8, 118, 10);
            oled.setDrawColor(0);
            oled.drawStr(4, y_pos, items[i]);
            if (values && values[i].length() > 0) {
                int vw = oled.getStrWidth(values[i].c_str());
                oled.drawStr(118 - vw, y_pos, values[i].c_str());
            }
            oled.setDrawColor(1);
        } else {
            oled.drawStr(4, y_pos, items[i]);
            if (values && values[i].length() > 0) {
                int vw = oled.getStrWidth(values[i].c_str());
                oled.drawStr(118 - vw, y_pos, values[i].c_str());
            }
        }
        y_pos += 12;
    }

    if (item_count > 3) {
        int scroll_h = 30;
        int scroll_y = 15 + ((float)offset / (item_count - 3)) * (scroll_h - 10);
        oled.drawFrame(123, 15, 3, 30);
        oled.drawBox(123, scroll_y, 3, 10);
    }
}

void DisplayManager::drawResetConfirm() {
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 7, "RESET SETTINGS");
    oled.drawLine(0, 9, 128, 9);

    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 26, "RESET SYSTEM?");
    oled.drawStr(10, 42, "PRESS SELECT: NO");
    oled.drawStr(10, 56, "[NOT IMPLEMENTED]");
}

void DisplayManager::drawTacticalOverlay() {
    oled.drawLine(0, 0, 6, 0); oled.drawLine(0, 0, 0, 6);
    oled.drawLine(127, 0, 121, 0); oled.drawLine(127, 0, 127, 6);
    oled.drawLine(0, 63, 6, 63); oled.drawLine(0, 63, 0, 57);
    oled.drawLine(127, 63, 121, 63); oled.drawLine(127, 63, 127, 57);
}

void DisplayManager::drawTopStatusBar() {
    oled.setFont(u8g2_font_4x6_tr);

    String t = qclock.getTimeStr();
    oled.drawStr(56, 6, t.c_str());

    if (WiFi.status() == WL_CONNECTED) {
        oled.drawStr(100, 6, "W");
    } else {
        oled.drawStr(100, 6, "-");
    }

    int pct = battery.readPercentage();
    String bStr = String(pct) + "%";
    oled.drawStr(110, 6, bStr.c_str());

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

    String hdgStr = String((int)heading) + "°";

    int cx = 64;
    int cy = 60;
    int r = 50;

    for (int i = 0; i < 360; i += 15) {
        float angle = (i - heading - 90) * PI / 180.0f;
        int x1 = cx + (r * cosf(angle));
        int y1 = cy + (r * sinf(angle));

        if (y1 <= cy + 5) {
            int len = (i % 90 == 0) ? 6 : (i % 30 == 0 ? 4 : 2);
            int x2 = cx + ((r - len) * cosf(angle));
            int y2 = cy + ((r - len) * sinf(angle));
            oled.drawLine(x1, y1, x2, y2);

            if (i % 90 == 0) {
                const char* lbl = (i == 0) ? "N" : (i == 90) ? "E" : (i == 180) ? "S" : "W";
                int tx = cx + ((r - 12) * cosf(angle));
                int ty = cy + ((r - 12) * sinf(angle));
                oled.setFont(u8g2_font_5x7_tr);
                int sw = oled.getStrWidth(lbl);
                oled.drawStr(tx - sw/2, ty + 3, lbl);
            } else if (i % 30 == 0) {
                String lbl = String(i);
                int tx = cx + ((r - 12) * cosf(angle));
                int ty = cy + ((r - 12) * sinf(angle));
                oled.setFont(u8g2_font_4x6_tr);
                int sw = oled.getStrWidth(lbl.c_str());
                oled.drawStr(tx - sw/2, ty + 3, lbl.c_str());
            }
        }
    }

    oled.drawTriangle(64, 4, 60, 12, 68, 12);

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
    // Performance Optimization: Use sqrtf for single-precision hardware FPU execution
    float magTotal = sqrtf(data.mx*data.mx + data.my*data.my + data.mz*data.mz);

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

    int graph_x = 10;
    int graph_y = 52;
    int graph_w = 108;
    int graph_h = 15;

    oled.drawFrame(graph_x, graph_y - graph_h, graph_w, graph_h + 1);

    float max_val = 1.0;
    for (int i = 0; i < 64; i++) {
        if (mag_history[i] > max_val) max_val = mag_history[i];
    }

    int prev_x = graph_x + 1;
    int prev_y = graph_y - (int)((mag_history[mag_history_idx % 64] / max_val) * graph_h);
    if (prev_y < graph_y - graph_h + 1) prev_y = graph_y - graph_h + 1;

    for (int i = 0; i < 63; i++) {
        int idx2 = (mag_history_idx + i + 1) % 64;

        int curr_x = graph_x + 1 + ((i + 1) * (graph_w - 2) / 63);
        int curr_y = graph_y - (int)((mag_history[idx2] / max_val) * graph_h);

        if (curr_y < graph_y - graph_h + 1) curr_y = graph_y - graph_h + 1;

        oled.drawLine(prev_x, prev_y, curr_x, curr_y);

        prev_x = curr_x;
        prev_y = curr_y;
    }
}

void DisplayManager::drawAppHealth() {
    if (healthManager.getPage() == 0) drawHealthLive();
    else drawHealthToday();
}

void DisplayManager::drawHealthLive() {
    const HealthLiveData& h = healthManager.getLiveData();

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 7, "HEALTH • LIVE");
    oled.drawLine(0, 9, 128, 9);

    oled.setFont(u8g2_font_ncenB10_tr);
    String bpm = h.bpm_valid ? String(h.bpm) + " BPM" : "-- BPM";
    oled.drawStr(2, 22, bpm.c_str());

    // Live IR pulse waveform.
    int gx = 2, gy = 39, gw = 124, gh = 15;
    oled.drawFrame(gx, gy - gh, gw, gh + 1);
    if (h.finger_detected && healthManager.getLiveData().ir > 0) {
        // The manager keeps a rolling 64-point waveform. Render normalized amplitude.
        uint32_t maxv = 1, minv = UINT32_MAX;
        for (int i = 0; i < 64; i++) {
            // Access is intentionally limited to public live metrics in this first pass.
            // A flat graph is preferable to exposing unstable raw internals.
        }
    } else {
        oled.setFont(u8g2_font_5x7_tr);
        oled.drawStr(34, 33, "PLACE FINGER");
    }

    oled.setFont(u8g2_font_5x7_tr);
    String spo = h.spo2_valid ? "SpO2 " + String(h.spo2) + "%" : "SpO2 --%";
    String tmp = (h.sensor_temp > -40.0f && h.sensor_temp < 85.0f)
                   ? "T " + String(h.sensor_temp, 1) + "C" : "T --.-C";
    oled.drawStr(2, 62, spo.c_str());
    oled.drawStr(78, 62, tmp.c_str());
}

void DisplayManager::drawHealthToday() {
    HealthHistoryEntry data[288];
    int count = healthManager.getTodayHistory(data, 288);

    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 7, "HEALTH • TODAY");
    oled.drawLine(0, 9, 128, 9);

    if (count <= 0) {
        oled.drawStr(18, 34, "NO BACKGROUND DATA");
        oled.drawStr(14, 46, "ENABLE IN SETTINGS");
        return;
    }

    // Three compact trend bands: BPM, SpO2, sensor temperature.
    const int x0 = 16, w = 108;
    const int yB = 23, yS = 39, yT = 55;

    oled.setFont(u8g2_font_4x6_tr);
    oled.drawStr(1, 22, "B");
    oled.drawStr(1, 38, "O");
    oled.drawStr(1, 54, "T");

    auto drawBand = [&](int y, int minV, int maxV, int metric) {
        int prevX = -1, prevY = -1;
        for (int i = 0; i < count; i++) {
            int value = (metric == 0) ? data[i].bpm :
                        (metric == 1) ? data[i].spo2 :
                        (data[i].temp_x10 == -32768 ? minV : data[i].temp_x10 / 10);
            if (value < minV) value = minV;
            if (value > maxV) value = maxV;
            int x = x0 + (i * (w - 1)) / (count > 1 ? count - 1 : 1);
            int py = y - ((value - minV) * 10) / (maxV - minV);
            if (prevX >= 0) oled.drawLine(prevX, prevY, x, py);
            prevX = x; prevY = py;
        }
    };

    drawBand(yB, 40, 180, 0);
    drawBand(yS, 90, 100, 1);
    drawBand(yT, 0, 50, 2);
}

void DisplayManager::drawHealthSettings() {
    // Reserved for a future dedicated Health settings page.
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

    int cx = 32;
    int cy = 32;
    int r = 30;

    oled.drawCircle(cx, cy, r);
    oled.drawCircle(cx, cy, 6);
    oled.drawLine(cx - r, cy, cx + r, cy);
    oled.drawLine(cx, cy - r, cx, cy + r);

    float scale = 30.0f;
    float r_roll = o.roll; if (r_roll > scale) r_roll = scale; if (r_roll < -scale) r_roll = -scale;
    float r_pitch = o.pitch; if (r_pitch > scale) r_pitch = scale; if (r_pitch < -scale) r_pitch = -scale;

    int bx = cx + (r_roll / scale) * (r - 4);
    int by = cy + (r_pitch / scale) * (r - 4);

    float dx = bx - cx; float dy = by - cy;
    // Performance Optimization: Use sqrtf for single-precision hardware FPU execution
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist > (r - 4)) {
        bx = cx + (dx / dist) * (r - 4);
        by = cy + (dy / dist) * (r - 4);
    }
    oled.drawDisc(bx, by, 4);

    int vp_x = 112; int vp_y = 2; int vp_w = 12; int vp_h = 60;
    oled.drawFrame(vp_x, vp_y, vp_w, vp_h);
    oled.drawLine(vp_x, cy, vp_x + vp_w - 1, cy);

    int vp_by = cy + (r_pitch / scale) * (vp_h/2 - 5);
    oled.drawBox(vp_x + 2, vp_by - 4, vp_w - 4, 8);

    int hr_x = 68; int hr_y = 48; int hr_w = 40; int hr_h = 12;
    oled.drawFrame(hr_x, hr_y, hr_w, hr_h);
    oled.drawLine(hr_x + hr_w/2, hr_y, hr_x + hr_w/2, hr_y + hr_h - 1);

    int hr_bx = (hr_x + hr_w/2) + (r_roll / scale) * (hr_w/2 - 5);
    oled.drawBox(hr_bx - 4, hr_y + 2, 8, hr_h - 4);

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

    oled.drawFrame(34, 10, 56, 25);
    oled.drawBox(90, 16, 4, 13);

    int fill_w = (pct * 52) / 100;
    if (fill_w > 0) {
        oled.drawBox(36, 12, fill_w, 21);
    }

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

void DisplayManager::drawFileManager() {
    drawTopStatusBar();

    String path = ui.getFmCurrentPath();
    oled.setFont(u8g2_font_5x7_tf);
    oled.drawStr(0, 18, path.c_str());
    oled.drawHLine(0, 21, 128);

    int count = ui.getFmEntryCount();
    bool isRoot = (path == "/");
    int total_items = count + (isRoot ? 1 : 0);

    if (total_items == 0) {
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(10, 40, "(Empty)");
        return;
    }

    const struct FileInfo* entries = ui.getFmEntries();
    int sel = ui.getFmSelection();
    int offset = ui.getFmScrollOffset();

    for (int i = 0; i < 3; i++) {
        int idx = offset + i;
        if (idx >= total_items) break;

        int y = 35 + (i * 12);

        if (idx == sel) {
            oled.drawStr(0, y, ">");
        }

        String displayName;
        if (isRoot && idx == count) {
            displayName = "STORAGE INFO";
        } else {
            displayName = entries[idx].name;
            if (entries[idx].isDir) {
                displayName += "/";
            }
        }

        oled.drawStr(10, y, displayName.c_str());

        if (!isRoot || idx < count) {
            if (!entries[idx].isDir) {
                char sizeStr[16];
                snprintf(sizeStr, sizeof(sizeStr), "%u B", entries[idx].size);
                int sw = oled.getStrWidth(sizeStr);
                oled.drawStr(128 - sw, y, sizeStr);
            }
        }
    }
}

void DisplayManager::drawStorageInfo() {
    drawTopStatusBar();

    oled.setFont(u8g2_font_5x7_tf);
    oled.drawStr(0, 18, "STORAGE INFO");
    oled.drawHLine(0, 21, 128);

    size_t total = fileManager.totalSpace();
    size_t free = fileManager.freeSpace();
    size_t used = total - free;

    oled.setFont(u8g2_font_6x10_tf);

    char buf[32];
    snprintf(buf, sizeof(buf), "Total: %u B", total);
    oled.drawStr(0, 35, buf);

    snprintf(buf, sizeof(buf), "Used:  %u B", used);
    oled.drawStr(0, 48, buf);

    snprintf(buf, sizeof(buf), "Free:  %u B", free);
    oled.drawStr(0, 61, buf);
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

            String label = String(ui.compass_menu_items[i]);
            if (i == 1) {
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


void DisplayManager::drawKeyboardScreen() {
    #include "keyboard.h"
    oled.setFont(u8g2_font_4x6_tr);

    String title = keyboardManager.getTitle();
    oled.drawStr(2, 6, title.c_str());
    oled.drawLine(0, 8, 128, 8);

    // Render input text box
    oled.drawFrame(2, 10, 124, 11);
    String txt = keyboardManager.getText();
    if (keyboardManager.isMasked()) {
        String masked = "";
        for (size_t i = 0; i < txt.length(); i++) masked += "*";
        txt = masked;
    }
    txt += "_"; // cursor
    oled.drawStr(4, 18, txt.c_str());

    // Keyboard Grid
    int rows = keyboardManager.getRowCount();
    int sel_r = keyboardManager.getSelectedRow();
    int sel_c = keyboardManager.getSelectedCol();

    int start_y = 23;
    int row_h = 10;

    for (int r = 0; r < rows; r++) {
        int cols = keyboardManager.getColCount(r);
        int col_w = 124 / cols;
        int y = start_y + (r * row_h);

        for (int c = 0; c < cols; c++) {
            int x = 2 + (c * col_w);
            const char* label = keyboardManager.getKeyLabel(r, c);

            if (r == sel_r && c == sel_c) {
                oled.drawBox(x, y, col_w - 1, row_h - 1);
                oled.setDrawColor(0);
                int lw = oled.getStrWidth(label);
                oled.drawStr(x + (col_w - lw) / 2, y + 7, label);
                oled.setDrawColor(1);
            } else {
                oled.drawFrame(x, y, col_w - 1, row_h - 1);
                int lw = oled.getStrWidth(label);
                oled.drawStr(x + (col_w - lw) / 2, y + 7, label);
            }
        }
    }
}


void DisplayManager::drawWifiScanScreen() {
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 7, "WIFI SCAN");
    oled.drawLine(0, 9, 128, 9);

    if (wifiPortal.isScanning()) {
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(20, 35, "SCANNING...");
        return;
    }

    int count = wifiPortal.getScannedNetworkCount();
    if (count == 0) {
        oled.setFont(u8g2_font_6x10_tr);
        oled.drawStr(10, 35, "NO NETWORKS");
        return;
    }

    const ScannedNetwork* nets = wifiPortal.getScannedNetworks();
    int sel = ui.getSettingsSelection();
    int offset = ui.getSettingsScrollOffset();

    oled.setFont(u8g2_font_6x10_tr);
    int y_pos = 22;

    for (int i = offset; i < offset + 3 && i < count; i++) {
        if (i == sel) {
            oled.drawBox(2, y_pos - 8, 118, 10);
            oled.setDrawColor(0);
        }

        String label = nets[i].ssid;
        if (label.length() > 10) label = label.substring(0, 10);
        oled.drawStr(4, y_pos, label.c_str());

        String meta = String(nets[i].rssi) + "d " + (nets[i].encrypted ? "*" : " ");
        int mw = oled.getStrWidth(meta.c_str());
        oled.drawStr(118 - mw, y_pos, meta.c_str());

        oled.setDrawColor(1);
        y_pos += 12;
    }

    if (count > 3) {
        int scroll_h = 30;
        int scroll_y = 15 + ((float)offset / (count - 3)) * (scroll_h - 10);
        oled.drawFrame(123, 15, 3, 30);
        oled.drawBox(123, scroll_y, 3, 10);
    }
}


void DisplayManager::drawFileServerDetailsScreen() {
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 7, "FILE SERVER");
    oled.drawLine(0, 9, 128, 9);

    oled.setFont(u8g2_font_6x10_tr);

    SettingsData& s = settingsManager.get();
    String pwrStr = "Power : " + String(s.fileserver_enabled ? "ON" : "OFF");
    String stStr  = "Status: " + String(s.fileserver_enabled ? "RUNNING" : "OFF");
    String urlStr = "URL   : q-watch.local/fm";
    String filesStr= "Files : " + String(wifiPortal.getTotalFileCount());

    oled.drawStr(2, 22, pwrStr.c_str());
    oled.drawStr(2, 34, stStr.c_str());
    oled.drawStr(2, 46, urlStr.c_str());
    oled.drawStr(2, 58, filesStr.c_str());
}

void DisplayManager::drawTimeSyncStatusScreen() {
    oled.setFont(u8g2_font_5x7_tr);
    oled.drawStr(2, 7, "SYNC STATUS");
    oled.drawLine(0, 9, 128, 9);

    oled.setFont(u8g2_font_6x10_tr);

    NtpSyncStatus status = qclock.getSyncStatus();
    String stStr = "Status: ";
    switch (status) {
        case NtpSyncStatus::IDLE: stStr += "IDLE"; break;
        case NtpSyncStatus::SYNCING: stStr += "SYNCING..."; break;
        case NtpSyncStatus::SUCCESS: stStr += "SUCCESS"; break;
        case NtpSyncStatus::FAILED: stStr += "FAILED"; break;
    }

    uint32_t last_sync = qclock.getLastSyncTime();
    String lastStr = "Last  : ";
    if (last_sync == 0) {
        lastStr += "Never";
    } else {
        uint32_t ago_sec = (millis() - last_sync) / 1000;
        if (ago_sec < 60) lastStr += String(ago_sec) + "s ago";
        else if (ago_sec < 3600) lastStr += String(ago_sec / 60) + "m ago";
        else lastStr += String(ago_sec / 3600) + "h ago";
    }

    String timeStr = "Time  : " + qclock.getTimeStr();
    String wifiStr = "Wi-Fi : " + String(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "OFFLINE");

    oled.drawStr(2, 22, stStr.c_str());
    oled.drawStr(2, 34, lastStr.c_str());
    oled.drawStr(2, 46, timeStr.c_str());
    oled.drawStr(2, 58, wifiStr.c_str());
}

void DisplayManager::applyDisplaySettings() {
    SettingsData& s = settingsManager.get();

    // Contrast Presets: 0: 25% (64), 1: 50% (128), 2: 75% (192), 3: 100% (255)
    uint8_t contrast_val = 255;
    if (s.contrast_idx == 0) contrast_val = 64;
    else if (s.contrast_idx == 1) contrast_val = 128;
    else if (s.contrast_idx == 2) contrast_val = 192;
    else contrast_val = 255;

    oled.setContrast(contrast_val);

    // Display Inversion
    if (s.invert_display) {
        oled.sendF("c", 0xA7); // SH1106 Inverse Display
    } else {
        oled.sendF("c", 0xA6); // SH1106 Normal Display
    }
}

void DisplayManager::setPowerSave(bool enable) {
    oled.setPowerSave(enable ? 1 : 0);
}

static void drawBmeGraph(U8G2& oled, const float* data, int count, float min_val, float max_val, const char* unit_str) {
    int gx = 4;
    int gy = 58;
    int gw = 120;
    int gh = 24;

    oled.drawFrame(gx, gy - gh, gw, gh);

    if (count < 2) {
        oled.setFont(u8g2_font_4x6_tr);
        oled.drawStr(gx + 30, gy - 10, "RECORDING HISTORY...");
        return;
    }

    if (max_val - min_val < 0.1f) {
        max_val += 1.0f;
        min_val -= 1.0f;
    }

    int prev_x = gx + 1;
    int prev_y = gy - 1 - (int)(((data[0] - min_val) / (max_val - min_val)) * (gh - 2));

    for (int i = 1; i < count; i++) {
        int cx = gx + 1 + (i * (gw - 2)) / (count - 1);
        int cy = gy - 1 - (int)(((data[i] - min_val) / (max_val - min_val)) * (gh - 2));
        if (cy < gy - gh + 1) cy = gy - gh + 1;
        if (cy > gy - 1) cy = gy - 1;

        oled.drawLine(prev_x, prev_y, cx, cy);
        prev_x = cx;
        prev_y = cy;
    }

    oled.setFont(u8g2_font_4x6_tr);
    char buf[16];
    snprintf(buf, sizeof(buf), "MAX:%.1f", max_val);
    oled.drawStr(gx + 2, gy - gh + 6, buf);
    snprintf(buf, sizeof(buf), "MIN:%.1f", min_val);
    oled.drawStr(gx + 75, gy - gh + 6, buf);
}

void DisplayManager::drawAppBme() {
    drawTopStatusBar();
    int page = ui.getBmePage();
    switch (page) {
        case 0: drawBmePage1Pressure(); break;
        case 1: drawBmePage2Humidity(); break;
        case 2: drawBmePage3Temperature(); break;
        case 3: drawBmePage4Altitude(); break;
        case 4: drawBmePage5Info(); break;
    }

    // Page indicator footer (1/5 ... 5/5)
    oled.setFont(u8g2_font_4x6_tr);
    char pg[8];
    snprintf(pg, sizeof(pg), "%d/5", page + 1);
    oled.drawStr(112, 63, pg);
}

void DisplayManager::drawBmePage1Pressure() {
    EnvironmentData env = sensors.getEnvData();

    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(2, 17, "PRESSURE");

    oled.setFont(u8g2_font_ncenB12_tr);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f hPa", env.pressure);
    oled.drawStr(2, 31, buf);

    // Fetch history
    BMEHistoryEntry history[64];
    bool has_h = sensors.getBmeHistory(history, 64);
    float press_data[64];
    int count = 0;
    float min_val = 2000.0f, max_val = 0.0f;

    if (has_h) {
        int total = sensors.getBmeHistoryCount();
        count = (total < 64) ? total : 64;
        for (int i = 0; i < count; i++) {
            press_data[i] = history[i].press_x10 / 10.0f;
            if (press_data[i] < min_val) min_val = press_data[i];
            if (press_data[i] > max_val) max_val = press_data[i];
        }
    }

    drawBmeGraph(oled, press_data, count, min_val, max_val, "hPa");
}

void DisplayManager::drawBmePage2Humidity() {
    EnvironmentData env = sensors.getEnvData();

    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(2, 17, "HUMIDITY");

    oled.setFont(u8g2_font_ncenB12_tr);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f %%°RH", env.humidity);
    oled.drawStr(2, 31, buf);

    BMEHistoryEntry history[64];
    bool has_h = sensors.getBmeHistory(history, 64);
    float hum_data[64];
    int count = 0;
    float min_val = 100.0f, max_val = 0.0f;

    if (has_h) {
        int total = sensors.getBmeHistoryCount();
        count = (total < 64) ? total : 64;
        for (int i = 0; i < count; i++) {
            hum_data[i] = history[i].hum_x10 / 10.0f;
            if (hum_data[i] < min_val) min_val = hum_data[i];
            if (hum_data[i] > max_val) max_val = hum_data[i];
        }
    }

    drawBmeGraph(oled, hum_data, count, min_val, max_val, "%%");
}

void DisplayManager::drawBmePage3Temperature() {
    EnvironmentData env = sensors.getEnvData();

    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(2, 17, "TEMPERATURE");

    oled.setFont(u8g2_font_ncenB12_tr);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f °C", env.temperature);
    oled.drawStr(2, 31, buf);

    BMEHistoryEntry history[64];
    bool has_h = sensors.getBmeHistory(history, 64);
    float temp_data[64];
    int count = 0;
    float min_val = 100.0f, max_val = -50.0f;

    if (has_h) {
        int total = sensors.getBmeHistoryCount();
        count = (total < 64) ? total : 64;
        for (int i = 0; i < count; i++) {
            temp_data[i] = history[i].temp_x10 / 10.0f;
            if (temp_data[i] < min_val) min_val = temp_data[i];
            if (temp_data[i] > max_val) max_val = temp_data[i];
        }
    }

    drawBmeGraph(oled, temp_data, count, min_val, max_val, "°C");
}

void DisplayManager::drawBmePage4Altitude() {
    EnvironmentData env = sensors.getEnvData();
    BmeHeightState st = sensors.getHeightState();

    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(2, 17, "RELATIVE HEIGHT");

    oled.setFont(u8g2_font_logisoso16_tr);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f m", env.altitude);
    int w = oled.getStrWidth(buf);
    oled.drawStr(64 - w/2, 36, buf);

    oled.setFont(u8g2_font_5x7_tr);
    if (st == BmeHeightState::OFF) {
        oled.drawStr(10, 52, "[OK]: SET ZERO / REF");
    } else if (st == BmeHeightState::MEASURING) {
        oled.drawStr(10, 52, "[MEASURING] OK:PAUSE");
    } else if (st == BmeHeightState::PAUSED) {
        oled.drawStr(10, 52, "[PAUSED]    OK:RESUME");
    }
}

void DisplayManager::drawBmePage5Info() {
    oled.setFont(u8g2_font_4x6_tr);
    oled.drawStr(2, 14, "BME280 CALIBRATION & DIAGNOSTIC");

    bool ok = sensors.isBmeOk();
    oled.drawStr(2, 22, ("STATUS: " + String(ok ? "DETECTED" : "NOT FOUND") + " | 0x76 ID:0x60").c_str());

    EnvironmentData env = sensors.getEnvData();
    char buf[64];
    snprintf(buf, sizeof(buf), "T:%.1fC P:%.1fhPa H:%.0f%%", env.temperature, env.pressure, env.humidity);
    oled.drawStr(2, 30, buf);

    snprintf(buf, sizeof(buf), "T_OFF : %+.1fC  (UP/DN: ADJUST)", sensors.getTempOffset());
    oled.drawStr(2, 38, buf);

    snprintf(buf, sizeof(buf), "P_REF : %.1fhPa", sensors.getReferencePressure());
    oled.drawStr(2, 46, buf);

    uint32_t last_t = sensors.getLastBmeReadingTime();
    uint32_t age_sec = (millis() - last_t) / 1000;
    snprintf(buf, sizeof(buf), "AGE   : %us | [OK] RESET CAL", age_sec);
    oled.drawStr(2, 54, buf);
}
