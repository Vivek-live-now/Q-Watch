#include "weather.h"
#include "display.h"
#include "ui_core.h"
#include "sensors.h"
#include "button_manager.h"
#include "hw_config.h"
#include "led_manager.h"
#include "sound_manager.h"
#include "settings_data.h"
#include "wifi_portal.h"
#include "clock.h"
#include "driver/rtc_io.h"

UICore ui;
String UICore::pending_selected_ssid = "";

UICore::UICore() :
    current_state(UIState::APP_HOME),
    return_state(UIState::APP_HOME),
    menu_selection(0),
    menu_scroll_offset(0),
    edit_value(5),
    settings_submenu(SettingsSubmenu::MAIN),
    settings_selection(0),
    settings_scroll_offset(0),
    toast_end_time(0),
    kb_callback(nullptr),
    compass_state(CompassState::PAGE_MAIN),
    compass_menu_selection(0),
    compass_menu_offset(0),
    motion_state(MotionState::PAGE_LEVEL),
    fm_current_path("/"),
    fm_selection(0),
    fm_scroll_offset(0),
    fm_entry_count(0),
    fm_entries(nullptr),
    last_activity_time(millis()),
    display_off(false),
    just_woke_display(false),
    needs_redraw(true) {
    toast_msg[0] = '\0';
}


void UICore::begin() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        Serial.println("Woke up from deep sleep via TIMER for BME280 logging...");
        sensors.begin();
        sensors.logBmeSample();

        // Re-arm timer and go back to sleep immediately without turning on display or WiFi
        SettingsData& s = settingsManager.get();
        uint32_t intervals_sec[] = {300, 600, 900, 1800, 3600}; // 5m, 10m, 15m, 30m, 1h
        uint32_t interval_sec = intervals_sec[s.bme_interval_idx];

        esp_sleep_enable_timer_wakeup((uint64_t)interval_sec * 1000000ULL);

        // Keep GPIO21 and GPIO8 wake sources active
        rtc_gpio_pullup_en((gpio_num_t)BTN_CANCEL);
        rtc_gpio_pulldown_dis((gpio_num_t)BTN_CANCEL);
        rtc_gpio_pullup_en((gpio_num_t)MPU_INT);
        rtc_gpio_pulldown_dis((gpio_num_t)MPU_INT);
        uint64_t wake_mask = (1ULL << BTN_CANCEL) | (1ULL << MPU_INT);
        esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);

        // Configure RTC timer wakeup for periodic sensor logging
    SettingsData& sleep_s = settingsManager.get();
    uint32_t sleep_intervals_sec[] = {300, 600, 900, 1800, 3600}; // 5m, 10m, 15m, 30m, 1h
    uint32_t sleep_interval_sec = sleep_intervals_sec[sleep_s.bme_interval_idx];
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_interval_sec * 1000000ULL);

    esp_deep_sleep_start();

    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {

        uint64_t status = esp_sleep_get_ext1_wakeup_status();
        if (status & (1ULL << BTN_CANCEL)) {
            Serial.println("Woke up from deep sleep via CANCEL button (GPIO21)!");
        } else if (status & (1ULL << MPU_INT)) {
            Serial.println("Woke up from deep sleep via IMU Motion (GPIO8)!");
        } else {
            Serial.println("Woke up from deep sleep via EXT1!");
        }
    } else if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Woke up from deep sleep via EXT0!");
    }
}

void UICore::showToast(const char* msg, uint32_t duration_ms) {
    strncpy(toast_msg, msg, sizeof(toast_msg) - 1);
    toast_msg[sizeof(toast_msg) - 1] = '\0';
    toast_end_time = millis() + duration_ms;
    needs_redraw = true;
}

void UICore::openKeyboard(const String& initial_text, const String& title, KeyboardMode mode, bool mask, int max_len, void (*on_complete)(bool success, const String& result)) {
    return_state = current_state;
    kb_callback = on_complete;
    keyboardManager.open(initial_text, title, mode, mask, max_len);
    current_state = UIState::APP_KEYBOARD;
    needs_redraw = true;
}

void UICore::loop() {
    // Check user button activity to keep awake or wake display
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent cancel_evt = btnManager.getEvent(BTN_ID_CANCEL);
    ComboEvent combo_evt = btnManager.getComboEvent();

    bool any_button = (up_evt != BTN_EVT_NONE || ok_evt != BTN_EVT_NONE || dn_evt != BTN_EVT_NONE || cancel_evt != BTN_EVT_NONE || combo_evt != COMBO_EVT_NONE);

    if (any_button) {
        if (display_off) {
            display_off = false;
            displayManager.setPowerSave(false);
            last_activity_time = millis();
            needs_redraw = true;
            return; // Consume first button press to wake display
        }
        last_activity_time = millis();
    }

    // Power timeouts check
    SettingsData& s = settingsManager.get();

    // 1. Display Timeout
    if (!display_off && s.display_timeout_idx < 4) {
        uint32_t disp_timeouts_ms[] = {10000, 30000, 60000, 300000};
        if (millis() - last_activity_time >= disp_timeouts_ms[s.display_timeout_idx]) {
            display_off = true;
            displayManager.setPowerSave(true);
        }
    }

    // 2. Sleep Timeout
    if (s.sleep_time_idx > 0 && s.sleep_time_idx < 5) {
        uint32_t sleep_timeouts_ms[] = {0, 60000, 300000, 900000, 1800000};
        if (millis() - last_activity_time >= sleep_timeouts_ms[s.sleep_time_idx]) {
            enterDeepSleep();
        }
    }

    if (toast_end_time > 0 && millis() > toast_end_time) {
        toast_msg[0] = '\0';
        toast_end_time = 0;
        needs_redraw = true;
    }

    if (current_state == UIState::SLEEPING) return;

    // Navigation rules for CANCEL and Long OK buttons across screens
    if (cancel_evt == BTN_EVT_SHORT_PRESS) {
        if (current_state == UIState::APP_HOME) {
            // Short CANCEL at HOME = Toggle display ON/OFF
            display_off = !display_off;
            displayManager.setPowerSave(display_off);
            needs_redraw = true;
            return;
        } else {
            // Short CANCEL at other screens = Back
            soundManager.playNavBack();
            if (current_state == UIState::MAIN_MENU) {
                current_state = UIState::APP_HOME;
            } else if (current_state == UIState::APP_SETTINGS) {
                if (settings_submenu == SettingsSubmenu::MAIN) {
                    current_state = UIState::MAIN_MENU;
                    menu_selection = 11;
                    menu_scroll_offset = 9;
                } else {
                    settings_submenu = SettingsSubmenu::MAIN;
                    settings_selection = 0;
                    settings_scroll_offset = 0;
                }
            } else {
                current_state = UIState::MAIN_MENU;
            }
            needs_redraw = true;
            return;
        }
    } else if (cancel_evt == BTN_EVT_LONG_PRESS) {
        if (current_state == UIState::APP_HOME) {
            // Long CANCEL at HOME = Deep Sleep
            enterDeepSleep();
            return;
        } else {
            // Long CANCEL on sub-screens = Return directly to HOME
            soundManager.playNavBack();
            current_state = UIState::APP_HOME;
            needs_redraw = true;
            return;
        }
    } else if (ok_evt == BTN_EVT_LONG_PRESS && current_state != UIState::APP_HOME) {
        // Long OK on non-home screens = Back
        soundManager.playNavBack();
        if (current_state == UIState::MAIN_MENU) {
            current_state = UIState::APP_HOME;
        } else if (current_state == UIState::APP_SETTINGS) {
            if (settings_submenu == SettingsSubmenu::MAIN) {
                current_state = UIState::MAIN_MENU;
                menu_selection = 11;
                menu_scroll_offset = 9;
            } else {
                settings_submenu = SettingsSubmenu::MAIN;
                settings_selection = 0;
                settings_scroll_offset = 0;
            }
        } else {
            current_state = UIState::MAIN_MENU;
        }
        needs_redraw = true;
        return;
    }

    switch (current_state) {
        case UIState::APP_HOME:
            handleHomeInput();
            break;
        case UIState::MAIN_MENU:
            handleMainMenuInput();
            break;
        case UIState::APP_SETTINGS:
            handleSettingsMenuInput();
            break;
        case UIState::APP_FILE_MANAGER:
            handleFileManagerInput();
            break;
        case UIState::APP_STORAGE_INFO:
            handleStorageInfoInput();
            break;
        case UIState::APP_KEYBOARD:
            handleKeyboardInput();
            break;
        case UIState::VALUE_EDIT:
            handleValueEditInput();
            break;
        case UIState::APP_COMPASS:
            handleCompassInput();
            break;
        case UIState::APP_MOTION:
            handleMotionInput();
            break;
        default:
            handleGenericAppInput();
            break;
    }
}

void UICore::handleKeyboardInput() {
    keyboardManager.handleInput();
    needs_redraw = true;

    if (!keyboardManager.isActive()) {
        bool success = (keyboardManager.getResult() == KeyboardResult::CONFIRMED);
        String text = keyboardManager.getText();

        current_state = return_state;
        needs_redraw = true;

        if (kb_callback) {
            kb_callback(success, text);
        }
    }
}

void UICore::processNavUp() {
    menu_selection--;
    if (menu_selection < 0) {
        menu_selection = 0;
    }

    if (menu_selection < menu_scroll_offset) {
        menu_scroll_offset = menu_selection;
    }
    soundManager.playNavMove();
    needs_redraw = true;
}

void UICore::processNavDown() {
    int max_items = (current_state == UIState::MAIN_MENU) ? MAIN_MENU_ITEM_COUNT : SETTINGS_MAIN_ITEM_COUNT;

    menu_selection++;
    if (menu_selection >= max_items) {
        menu_selection = max_items - 1;
    }

    if (menu_selection >= menu_scroll_offset + 3) {
        menu_scroll_offset = menu_selection - 2;
    }
    soundManager.playNavMove();
    needs_redraw = true;
}

void UICore::handleHomeInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        current_state = UIState::MAIN_MENU;
        menu_selection = 0;
        menu_scroll_offset = 0;
        needs_redraw = true;
    }
}

void UICore::handleMainMenuInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) processNavUp();

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) processNavDown();

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        switch(menu_selection) {
            case 0: current_state = UIState::APP_HOME; break;
            case 1: current_state = UIState::APP_CLOCK; break;
            case 2: current_state = UIState::APP_WEATHER; break;
            case 3: current_state = UIState::APP_COMPASS; compass_state = CompassState::PAGE_MAIN; break;
            case 4: current_state = UIState::APP_HEALTH; break;
            case 5: current_state = UIState::APP_MOTION; motion_state = MotionState::PAGE_LEVEL; break;
            case 6: current_state = UIState::APP_IR; break;
            case 7: current_state = UIState::APP_ALTIMETER; break;
            case 8: current_state = UIState::APP_BATTERY; break;
            case 9: current_state = UIState::APP_LED; break;
            case 10: current_state = UIState::APP_FILE_MANAGER; fm_current_path = "/"; loadDirectory("/"); break;
            case 11:
                current_state = UIState::APP_SETTINGS;
                settings_submenu = SettingsSubmenu::MAIN;
                settings_selection = 0;
                settings_scroll_offset = 0;
                break;
            case 12: current_state = UIState::APP_ABOUT; break;
        }
        needs_redraw = true;
    }
}

void UICore::handleSettingsMenuInput() {
    switch (settings_submenu) {
        case SettingsSubmenu::MAIN: handleSettingsMainInput(); break;
        case SettingsSubmenu::CONNECTIVITY: handleConnectivityInput(); break;
        case SettingsSubmenu::WIFI_DETAILS: handleWifiDetailsInput(); break;
        case SettingsSubmenu::WIFI_SCAN: handleWifiScanInput(); break;
        case SettingsSubmenu::FILE_SERVER_DETAILS: handleFileServerDetailsInput(); break;
        case SettingsSubmenu::TIME: handleTimeInput(); break;
        case SettingsSubmenu::TIME_SYNC_STATUS: handleTimeSyncStatusInput(); break;
        case SettingsSubmenu::POWER: handlePowerInput(); break;
        case SettingsSubmenu::SUB_DISPLAY: handleDisplayInput(); break;
        case SettingsSubmenu::SENSORS: handleSensorsInput(); break;
        case SettingsSubmenu::SYSTEM: handleSystemInput(); break;
        case SettingsSubmenu::RESET_CONFIRM: handleResetConfirmInput(); break;
    }
}

void UICore::handleSettingsMainInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        settings_selection--;
        if (settings_selection < 0) settings_selection = 0;
        if (settings_selection < settings_scroll_offset) settings_scroll_offset = settings_selection;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        settings_selection++;
        if (settings_selection >= SETTINGS_MAIN_ITEM_COUNT) settings_selection = SETTINGS_MAIN_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        switch (settings_selection) {
            case 0: settings_submenu = SettingsSubmenu::CONNECTIVITY; break;
            case 1: settings_submenu = SettingsSubmenu::TIME; break;
            case 2: settings_submenu = SettingsSubmenu::POWER; break;
            case 3: settings_submenu = SettingsSubmenu::SUB_DISPLAY; break;
            case 4: settings_submenu = SettingsSubmenu::SENSORS; break;
            case 5: settings_submenu = SettingsSubmenu::SYSTEM; break;
        }
        settings_selection = 0;
        settings_scroll_offset = 0;
        needs_redraw = true;
    }
}

void UICore::handleConnectivityInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        settings_selection--;
        if (settings_selection < 0) settings_selection = 0;
        if (settings_selection < settings_scroll_offset) settings_scroll_offset = settings_selection;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        settings_selection++;
        if (settings_selection >= CONNECTIVITY_ITEM_COUNT) settings_selection = CONNECTIVITY_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (settings_selection == 0) {
            settings_submenu = SettingsSubmenu::WIFI_DETAILS;
        } else if (settings_selection == 1) {
            settings_submenu = SettingsSubmenu::WIFI_SCAN;
            settings_selection = 0;
            settings_scroll_offset = 0;
            wifiPortal.startScan();
        } else if (settings_selection == 2) {
            SettingsData& s = settingsManager.get();
            s.ble_enabled = !s.ble_enabled;
            settingsManager.save();
        } else if (settings_selection == 3) {
            settings_submenu = SettingsSubmenu::FILE_SERVER_DETAILS;
        }
        needs_redraw = true;
    }
}

static void onWifiPasswordEntered(bool success, const String& password) {
    if (success) {
        wifiPortal.connectToNetwork(UICore::pending_selected_ssid, password);
        ui.showToast("[CONNECTING...]", 2000);
    }
}

void UICore::handleWifiScanInput() {
    if (wifiPortal.isScanning()) {
        return;
    }

    int count = wifiPortal.getScannedNetworkCount();

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        settings_selection--;
        if (settings_selection < 0) settings_selection = 0;
        if (settings_selection < settings_scroll_offset) settings_scroll_offset = settings_selection;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        settings_selection++;
        if (settings_selection >= count) settings_selection = (count > 0) ? count - 1 : 0;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        if (count > 0 && settings_selection < count) {
            soundManager.playNavSelect();
            const ScannedNetwork* nets = wifiPortal.getScannedNetworks();
            ScannedNetwork target = nets[settings_selection];
            pending_selected_ssid = target.ssid;

            if (target.encrypted) {
                openKeyboard("", "Pass for " + target.ssid, KeyboardMode::ALPHA, true, 32, onWifiPasswordEntered);
            } else {
                wifiPortal.connectToNetwork(target.ssid, "");
                showToast("[CONNECTING...]", 2000);
                settings_submenu = SettingsSubmenu::WIFI_DETAILS;
            }
        }
    }
}

void UICore::handleWifiDetailsInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        s.wifi_enabled = !s.wifi_enabled;
        settingsManager.save();

        if (s.wifi_enabled) {
            wifiPortal.enableWifi();
        } else {
            wifiPortal.disableWifi();
        }
        needs_redraw = true;
    }
}

void UICore::handleTimeInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        settings_selection--;
        if (settings_selection < 0) settings_selection = 0;
        if (settings_selection < settings_scroll_offset) settings_scroll_offset = settings_selection;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        settings_selection++;
        if (settings_selection >= TIME_ITEM_COUNT) settings_selection = TIME_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) {
            if (WiFi.status() == WL_CONNECTED) {
                qclock.syncNtp();
                showToast("[SYNCING...]", 1500);
            } else {
                showToast("[NO WI-FI]", 1500);
            }
        } else if (settings_selection == 1) {
            settings_submenu = SettingsSubmenu::TIME_SYNC_STATUS;
        } else if (settings_selection == 2) {
            s.auto_sync = !s.auto_sync;
            settingsManager.save();
        } else if (settings_selection == 3) {
            s.timezone_idx = (s.timezone_idx + 1) % TIMEZONE_OPTION_COUNT;
            settingsManager.save();
            qclock.setTimezoneIdx(s.timezone_idx);
        } else if (settings_selection == 4) {
            s.format_24hr = !s.format_24hr;
            settingsManager.save();
        }
        needs_redraw = true;
    }
}

void UICore::handleTimeSyncStatusInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavBack();
        settings_submenu = SettingsSubmenu::TIME;
        settings_selection = 1;
        settings_scroll_offset = 0;
        needs_redraw = true;
    }
}

void UICore::handlePowerInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        settings_selection--;
        if (settings_selection < 0) settings_selection = 0;
        if (settings_selection < settings_scroll_offset) settings_scroll_offset = settings_selection;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        settings_selection++;
        if (settings_selection >= POWER_ITEM_COUNT) settings_selection = POWER_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) {
            s.display_timeout_idx = (s.display_timeout_idx + 1) % DISPLAY_TIMEOUT_COUNT;
            settingsManager.save();
        } else if (settings_selection == 1) {
            s.sleep_time_idx = (s.sleep_time_idx + 1) % SLEEP_TIMEOUT_COUNT;
            settingsManager.save();
        } else if (settings_selection == 2) {
            s.wifi_auto_off_idx = (s.wifi_auto_off_idx + 1) % WIFI_AUTO_OFF_COUNT;
            settingsManager.save();
        } else if (settings_selection == 3) {
            s.low_power = !s.low_power;
            settingsManager.save();
        }
        needs_redraw = true;
    }
}

void UICore::handleDisplayInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        settings_selection--;
        if (settings_selection < 0) settings_selection = 0;
        if (settings_selection < settings_scroll_offset) settings_scroll_offset = settings_selection;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        settings_selection++;
        if (settings_selection >= DISPLAY_ITEM_COUNT) settings_selection = DISPLAY_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) {
            s.contrast_idx = (s.contrast_idx + 1) % CONTRAST_OPTION_COUNT;
            settingsManager.save();
            displayManager.applyDisplaySettings();
        } else if (settings_selection == 1) {
            s.invert_display = !s.invert_display;
            settingsManager.save();
            displayManager.applyDisplaySettings();
        } else if (settings_selection == 2) {
            s.ui_option_idx = (s.ui_option_idx + 1) % UI_OPTIONS_COUNT;
            settingsManager.save();
        }
        needs_redraw = true;
    }
}

void UICore::handleSensorsInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        settings_selection--;
        if (settings_selection < 0) settings_selection = 0;
        if (settings_selection < settings_scroll_offset) settings_scroll_offset = settings_selection;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        settings_selection++;
        if (settings_selection >= SENSORS_ITEM_COUNT) settings_selection = SENSORS_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (settings_selection == 0) {
            showToast("[COMPASS CAL]", 1500);
        } else if (settings_selection == 1) {
            showToast("[IMU CAL]", 1500);
        } else if (settings_selection == 2) {
            showToast("[STATUS: OK]", 1500);
        }
        needs_redraw = true;
    }
}

void UICore::handleSystemInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        settings_selection--;
        if (settings_selection < 0) settings_selection = 0;
        if (settings_selection < settings_scroll_offset) settings_scroll_offset = settings_selection;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        settings_selection++;
        if (settings_selection >= SYSTEM_ITEM_COUNT) settings_selection = SYSTEM_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (settings_selection == 0) {
            current_state = UIState::APP_STORAGE_INFO;
        } else if (settings_selection == 1) {
            settings_submenu = SettingsSubmenu::RESET_CONFIRM;
        }
        needs_redraw = true;
    }
}

void UICore::handleResetConfirmInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavBack();
        showToast("[CANCELLED]", 1200);
        settings_submenu = SettingsSubmenu::SYSTEM;
        settings_selection = 1;
        settings_scroll_offset = 0;
        needs_redraw = true;
    }
}

void UICore::handleGenericAppInput() {
    // Handled globally via CANCEL / Long OK
}

void UICore::freeFileManager() {
    if (fm_entries) {
        delete[] fm_entries;
        fm_entries = nullptr;
    }
    fm_entry_count = 0;
}

void UICore::loadDirectory(const String& path) {
    freeFileManager();
    fm_entries = new FileInfo[32];
    if (!fm_entries) return;

    fm_entry_count = fileManager.listDir(path, fm_entries, 32);
    fm_selection = 0;
    fm_scroll_offset = 0;
}

void UICore::handleFileManagerInput() {
    int total_entries = fm_entry_count + (fm_current_path == "/" ? 1 : 0);

    if (btnManager.getEvent(BTN_ID_UP) == BTN_EVT_SHORT_PRESS) {
        if (total_entries > 0 && fm_selection > 0) {
            fm_selection--;
            if (fm_selection < fm_scroll_offset) fm_scroll_offset = fm_selection;
            needs_redraw = true;
        }
    } else if (btnManager.getEvent(BTN_ID_DN) == BTN_EVT_SHORT_PRESS) {
        if (total_entries > 0 && fm_selection < total_entries - 1) {
            fm_selection++;
            if (fm_selection >= fm_scroll_offset + 3) fm_scroll_offset = fm_selection - 2;
            needs_redraw = true;
        }
    } else if (btnManager.getEvent(BTN_ID_OK) == BTN_EVT_SHORT_PRESS) {
        if (fm_current_path == "/" && fm_selection == fm_entry_count) {
            current_state = UIState::APP_STORAGE_INFO;
        } else if (fm_selection < fm_entry_count) {
            if (fm_entries[fm_selection].isDir) {
                String next_path = fm_current_path;
                if (!next_path.endsWith("/")) next_path += "/";
                next_path += fm_entries[fm_selection].name;
                fm_current_path = next_path;
                loadDirectory(fm_current_path);
            }
        }
        needs_redraw = true;
    }
}

void UICore::handleStorageInfoInput() {
    // Handled globally via CANCEL / Long OK
}

void UICore::handleValueEditInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (edit_value > 0) { edit_value--; needs_redraw = true; }
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (edit_value < 10) { edit_value++; needs_redraw = true; }
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        current_state = UIState::APP_SETTINGS;
        needs_redraw = true;
    }
}

void UICore::enterDeepSleep() {
    current_state = UIState::SLEEPING;
    Serial.println("Going to deep sleep...");
    delay(100);

    // 1. Enable MPU-6500 motion interrupt for raise-to-wake
    sensors.enableMotionInterruptForSleep();

    // 2. Configure RTC internal pull-ups for GPIO 21 (CANCEL) and GPIO 8 (MPU INT)
    rtc_gpio_pullup_en((gpio_num_t)BTN_CANCEL);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_CANCEL);

    rtc_gpio_pullup_en((gpio_num_t)MPU_INT);
    rtc_gpio_pulldown_dis((gpio_num_t)MPU_INT);

    // 3. Configure EXT1 wakeup on ANY LOW for GPIO 21 and GPIO 8
    uint64_t wake_mask = (1ULL << BTN_CANCEL) | (1ULL << MPU_INT);
    esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);

    // Configure RTC timer wakeup for periodic sensor logging
    SettingsData& sleep_s = settingsManager.get();
    uint32_t sleep_intervals_sec[] = {300, 600, 900, 1800, 3600}; // 5m, 10m, 15m, 30m, 1h
    uint32_t sleep_interval_sec = sleep_intervals_sec[sleep_s.bme_interval_idx];
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_interval_sec * 1000000ULL);

    esp_deep_sleep_start();

}

void UICore::handleCompassInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (compass_state == CompassState::PAGE_MAIN) {
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_METRICS;
            needs_redraw = true;
        }
    }
    else if (compass_state == CompassState::PAGE_METRICS) {
        if (up_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_MAIN;
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
    }
    else if (compass_state == CompassState::PAGE_CAL_MENU) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            compass_menu_selection--;
            if (compass_menu_selection < 0) compass_menu_selection = 0;
            if (compass_menu_selection < compass_menu_offset) compass_menu_offset = compass_menu_selection;
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            compass_menu_selection++;
            if (compass_menu_selection >= COMPASS_MENU_ITEM_COUNT) compass_menu_selection = COMPASS_MENU_ITEM_COUNT - 1;
            if (compass_menu_selection >= compass_menu_offset + 3) compass_menu_offset = compass_menu_selection - 2;
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (compass_menu_selection == 0) {
                sensors.startMagCalibration();
                compass_state = CompassState::CAL_SWEEP;
            } else if (compass_menu_selection == 1) {
                MagCalibration cal = sensors.getMagCalibration();
                cal.orientation_mode = (cal.orientation_mode + 1) % 4;
                sensors.saveMagCalibration(cal);
            } else if (compass_menu_selection == 2) {
                MagCalibration cal = sensors.getMagCalibration();
                cal.invert_z = !cal.invert_z;
                sensors.saveMagCalibration(cal);
            } else if (compass_menu_selection == 3) {
                compass_state = CompassState::CAL_DECLINATION;
            } else if (compass_menu_selection == 4) {
                compass_state = CompassState::CAL_TELEMETRY;
            } else if (compass_menu_selection == 5) {
                sensors.factoryResetCalibration();
            }
            needs_redraw = true;
        }
    }
    else if (compass_state == CompassState::CAL_SWEEP) {
        if (sensors.getCalState() == MagCalState::RESULT) {
            compass_state = CompassState::CAL_RESULT;
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            sensors.cancelMagCalibration();
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
        needs_redraw = true;
    }
    else if (compass_state == CompassState::CAL_RESULT) {
        if (up_evt == BTN_EVT_SHORT_PRESS) {
            sensors.saveCurrentCalibration();
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
            sensors.cancelMagCalibration();
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
    }
    else if (compass_state == CompassState::CAL_TELEMETRY) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
        needs_redraw = true;
    }
    else if (compass_state == CompassState::CAL_DECLINATION) {
        MagCalibration cal = sensors.getMagCalibration();
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            cal.declination += 0.1f;
            if (cal.declination > 180.0f) cal.declination = -180.0f;
            sensors.saveMagCalibration(cal);
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            cal.declination -= 0.1f;
            if (cal.declination < -180.0f) cal.declination = 180.0f;
            sensors.saveMagCalibration(cal);
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
    }
}

void UICore::handleMotionInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (motion_state == MotionState::PAGE_LEVEL) {
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            motion_state = MotionState::PAGE_DATA;
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            sensors.zeroLevel();
            needs_redraw = true;
        }
    }
    else if (motion_state == MotionState::PAGE_DATA) {
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            motion_state = MotionState::PAGE_SETTINGS;
            compass_menu_selection = 0;
            needs_redraw = true;
        } else if (up_evt == BTN_EVT_SHORT_PRESS) {
            motion_state = MotionState::PAGE_LEVEL;
            needs_redraw = true;
        }
    }
    else if (motion_state == MotionState::PAGE_SETTINGS) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            compass_menu_selection--;
            if (compass_menu_selection < 0) compass_menu_selection = 0;
            if (compass_menu_selection < compass_menu_offset) compass_menu_offset = compass_menu_selection;
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            compass_menu_selection++;
            if (compass_menu_selection >= MOTION_MENU_ITEM_COUNT) compass_menu_selection = MOTION_MENU_ITEM_COUNT - 1;
            if (compass_menu_selection >= compass_menu_offset + 3) compass_menu_offset = compass_menu_selection - 2;
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (compass_menu_selection == 0) sensors.setImuSwapXY(!sensors.getImuSwapXY());
            else if (compass_menu_selection == 1) sensors.setImuInvX(!sensors.getImuInvX());
            else if (compass_menu_selection == 2) sensors.setImuInvY(!sensors.getImuInvY());
            else if (compass_menu_selection == 3) sensors.setImuInvZ(!sensors.getImuInvZ());
            else if (compass_menu_selection == 4) sensors.calibrateAccel();
            else if (compass_menu_selection == 5) sensors.zeroLevel();
            needs_redraw = true;
        }
    }
}

void UICore::handleAudioInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        audio_menu_selection--;
        if (audio_menu_selection < 0) audio_menu_selection = 0;
        if (audio_menu_selection < audio_menu_offset) audio_menu_offset = audio_menu_selection;
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        audio_menu_selection++;
        if (audio_menu_selection >= AUDIO_MENU_ITEM_COUNT) audio_menu_selection = AUDIO_MENU_ITEM_COUNT - 1;
        if (audio_menu_selection >= audio_menu_offset + 3) audio_menu_offset = audio_menu_selection - 2;
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (audio_menu_selection == 0) {
            soundManager.setMasterSwitch(!soundManager.isMasterSwitchOn());
        } else if (audio_menu_selection == 1) {
            int s = (int)soundManager.getStyle();
            s = (s + 1) % 4;
            soundManager.setStyle((SoundStyle)s);
        }
        needs_redraw = true;
    }
}

void UICore::handleFileServerDetailsInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        s.fileserver_enabled = !s.fileserver_enabled;
        settingsManager.save();
        needs_redraw = true;
    }
}

void UICore::registerActivity() {
    last_activity_time = millis();
    if (display_off) {
        display_off = false;
        just_woke_display = true;
        displayManager.setPowerSave(false);
        needs_redraw = true;
    }
}

void UICore::handleBmeInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    // BME280 App has 5 Pages (0: Pressure, 1: Humidity, 2: Temp, 3: Altitude/Height, 4: Calibration/Info)
    if (bme_page == 3) { // Page 4: Altitude / Relative Height
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            sensors.toggleHeightMeasurement();
            needs_redraw = true;
            return;
        }
    } else if (bme_page == 4) { // Page 5: Calibration / Diagnostic Options
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            sensors.resetBmeCalibration();
            showToast("[CAL RESET]", 1500);
            needs_redraw = true;
            return;
        } else if (up_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavMove();
            sensors.setTempOffset(sensors.getTempOffset() + 0.5f);
            showToast("[TEMP OFF +0.5]", 1200);
            needs_redraw = true;
            return;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavMove();
            sensors.setTempOffset(sensors.getTempOffset() - 0.5f);
            showToast("[TEMP OFF -0.5]", 1200);
            needs_redraw = true;
            return;
        }
    }

    // Page navigation via UP / DOWN
    if (up_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        bme_page = (bme_page + 4) % 5;
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        bme_page = (bme_page + 1) % 5;
        needs_redraw = true;
    }
}

void UICore::handleWeatherInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    // Page 6: Weather / Data Settings (Refresh & Logging Interval)
    if (weather_page == 5) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            SettingsData& s = settingsManager.get();
            s.bme_interval_idx = (s.bme_interval_idx + 1) % BME_INTERVAL_COUNT;
            settingsManager.save();
            showToast("[INTERVAL UPDATED]", 1200);
            needs_redraw = true;
            return;
        }
    } else if (weather_page == 1) { // Page 2: OWM data
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            weather.forceUpdate();
            showToast("[FETCHING OWM...]", 1200);
            needs_redraw = true;
            return;
        }
    }

    // Page Navigation via UP / DOWN
    if (up_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        weather_page = (weather_page + 5) % 6;
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        weather_page = (weather_page + 1) % 6;
        needs_redraw = true;
    }
}
