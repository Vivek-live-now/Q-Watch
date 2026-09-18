#include "ui_core.h"
#include "sensors.h"
#include "button_manager.h"
#include "hw_config.h"
#include "led_manager.h"
#include "sound_manager.h"
#include "settings_data.h"
#include "driver/rtc_io.h"

UICore ui;

UICore::UICore() :
    current_state(UIState::APP_HOME),
    menu_selection(0),
    menu_scroll_offset(0),
    edit_value(5),
    settings_submenu(SettingsSubmenu::MAIN),
    settings_selection(0),
    settings_scroll_offset(0),
    toast_end_time(0),
    compass_state(CompassState::PAGE_MAIN),
    compass_menu_selection(0),
    compass_menu_offset(0),
    motion_state(MotionState::PAGE_LEVEL),
    fm_current_path("/"),
    fm_selection(0),
    fm_scroll_offset(0),
    fm_entry_count(0),
    fm_entries(nullptr),
    needs_redraw(true) {
    toast_msg[0] = '\0';
}

void UICore::begin() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Woke up from deep sleep via SELECT button (GPIO21)!");
    }
}

void UICore::showToast(const char* msg, uint32_t duration_ms) {
    strncpy(toast_msg, msg, sizeof(toast_msg) - 1);
    toast_msg[sizeof(toast_msg) - 1] = '\0';
    toast_end_time = millis() + duration_ms;
    needs_redraw = true;
}

void UICore::loop() {
    if (toast_end_time > 0 && millis() > toast_end_time) {
        toast_msg[0] = '\0';
        toast_end_time = 0;
        needs_redraw = true;
    }

    if (current_state == UIState::SLEEPING) return;

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
    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
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

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
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
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        current_state = UIState::APP_HOME;
        needs_redraw = true;
    }
}

void UICore::handleSettingsMenuInput() {
    switch (settings_submenu) {
        case SettingsSubmenu::MAIN: handleSettingsMainInput(); break;
        case SettingsSubmenu::CONNECTIVITY: handleConnectivityInput(); break;
        case SettingsSubmenu::TIME: handleTimeInput(); break;
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

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
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
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        current_state = UIState::MAIN_MENU;
        menu_selection = 11;
        menu_scroll_offset = 9;
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

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) s.wifi_enabled = !s.wifi_enabled;
        else if (settings_selection == 1) s.ble_enabled = !s.ble_enabled;
        else if (settings_selection == 2) s.fileserver_enabled = !s.fileserver_enabled;
        settingsManager.save();
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        settings_submenu = SettingsSubmenu::MAIN;
        settings_selection = 0;
        settings_scroll_offset = 0;
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

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) {
            showToast("[SYNCING...]", 1500);
        } else if (settings_selection == 1) {
            s.auto_sync = !s.auto_sync;
            settingsManager.save();
        } else if (settings_selection == 2) {
            s.timezone_idx = (s.timezone_idx + 1) % TIMEZONE_OPTION_COUNT;
            settingsManager.save();
        } else if (settings_selection == 3) {
            s.format_24hr = !s.format_24hr;
            settingsManager.save();
        }
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        settings_submenu = SettingsSubmenu::MAIN;
        settings_selection = 1; // TIME in top-level
        settings_scroll_offset = 0;
        needs_redraw = true;
    }
}

void UICore::handlePowerInput() {
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
        if (settings_selection >= POWER_ITEM_COUNT) settings_selection = POWER_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) {
            s.display_timeout_idx = (s.display_timeout_idx + 1) % TIMEOUT_OPTION_COUNT;
            settingsManager.save();
        } else if (settings_selection == 1) {
            s.sleep_time_idx = (s.sleep_time_idx + 1) % TIMEOUT_OPTION_COUNT;
            settingsManager.save();
        } else if (settings_selection == 2) {
            s.low_power = !s.low_power;
            settingsManager.save();
        }
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        settings_submenu = SettingsSubmenu::MAIN;
        settings_selection = 2; // POWER
        settings_scroll_offset = 0;
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

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) {
            s.contrast += 25;
            if (s.contrast > 100) s.contrast = 25;
            settingsManager.save();
        } else if (settings_selection == 1) {
            s.invert_display = !s.invert_display;
            settingsManager.save();
        } else if (settings_selection == 2) {
            s.ui_option_idx = (s.ui_option_idx + 1) % UI_OPTIONS_COUNT;
            settingsManager.save();
        }
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        settings_submenu = SettingsSubmenu::MAIN;
        settings_selection = 3; // DISPLAY
        settings_scroll_offset = 1;
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

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (settings_selection == 0) {
            showToast("[COMPASS CAL]", 1500);
        } else if (settings_selection == 1) {
            showToast("[IMU CAL]", 1500);
        } else if (settings_selection == 2) {
            showToast("[STATUS: OK]", 1500);
        }
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        settings_submenu = SettingsSubmenu::MAIN;
        settings_selection = 4; // SENSORS
        settings_scroll_offset = 2;
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

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (settings_selection == 0) {
            current_state = UIState::APP_STORAGE_INFO;
        } else if (settings_selection == 1) {
            settings_submenu = SettingsSubmenu::RESET_CONFIRM;
        }
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        settings_submenu = SettingsSubmenu::MAIN;
        settings_selection = 5; // SYSTEM
        settings_scroll_offset = 3;
        needs_redraw = true;
    }
}

void UICore::handleResetConfirmInput() {
    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS || sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        showToast("[CANCELLED]", 1200);
        settings_submenu = SettingsSubmenu::SYSTEM;
        settings_selection = 1;
        settings_scroll_offset = 0;
        needs_redraw = true;
    }
}

void UICore::handleGenericAppInput() {
    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_LONG_PRESS) {
        current_state = UIState::APP_HOME;
        needs_redraw = true;
    }
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
    } else if (btnManager.getEvent(BTN_ID_SEL) == BTN_EVT_SHORT_PRESS) {
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
    } else if (btnManager.getEvent(BTN_ID_SEL) == BTN_EVT_LONG_PRESS) {
        if (fm_current_path == "/") {
            freeFileManager();
            current_state = UIState::MAIN_MENU;
            menu_selection = 10;
            menu_scroll_offset = 8;
        } else {
            int last_slash = fm_current_path.lastIndexOf('/', fm_current_path.length() - 2);
            if (last_slash == -1 || last_slash == 0) {
                fm_current_path = "/";
            } else {
                fm_current_path = fm_current_path.substring(0, last_slash);
            }
            loadDirectory(fm_current_path);
        }
        needs_redraw = true;
    }
}

void UICore::handleStorageInfoInput() {
    if (btnManager.getEvent(BTN_ID_SEL) == BTN_EVT_LONG_PRESS) {
        if (current_state == UIState::APP_STORAGE_INFO) {
            current_state = UIState::APP_SETTINGS;
            settings_submenu = SettingsSubmenu::SYSTEM;
            settings_selection = 0;
            settings_scroll_offset = 0;
            needs_redraw = true;
        }
    }
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

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS || sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavSelect();
        current_state = UIState::APP_SETTINGS;
        needs_redraw = true;
    }
}

void UICore::enterDeepSleep() {
    current_state = UIState::SLEEPING;
    Serial.println("Going to sleep now...");
    delay(100);

    rtc_gpio_pullup_en((gpio_num_t)BTN_SEL);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_SEL);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_SEL, 0);

    esp_deep_sleep_start();
}

void UICore::handleCompassInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);

    if (compass_state == CompassState::PAGE_MAIN) {
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_METRICS;
            needs_redraw = true;
        } else if (sel_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavBack();
            current_state = UIState::APP_HOME;
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
        } else if (sel_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavBack();
            current_state = UIState::APP_HOME;
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
        } else if (sel_evt == BTN_EVT_SHORT_PRESS) {
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
        } else if (sel_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavBack();
            compass_state = CompassState::PAGE_METRICS;
            needs_redraw = true;
        }
    }
    else if (compass_state == CompassState::CAL_SWEEP) {
        if (sensors.getCalState() == MagCalState::RESULT) {
            compass_state = CompassState::CAL_RESULT;
            needs_redraw = true;
        } else if (sel_evt == BTN_EVT_SHORT_PRESS || sel_evt == BTN_EVT_LONG_PRESS) {
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
        if (sel_evt == BTN_EVT_LONG_PRESS || sel_evt == BTN_EVT_SHORT_PRESS) {
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
        } else if (sel_evt == BTN_EVT_LONG_PRESS || sel_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
    }
}

void UICore::handleMotionInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);

    if (motion_state == MotionState::PAGE_LEVEL) {
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            motion_state = MotionState::PAGE_DATA;
            needs_redraw = true;
        } else if (sel_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            sensors.zeroLevel();
            needs_redraw = true;
        } else if (sel_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavBack();
            current_state = UIState::APP_HOME;
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
        } else if (sel_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavBack();
            current_state = UIState::APP_HOME;
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
        } else if (sel_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (compass_menu_selection == 0) sensors.setImuSwapXY(!sensors.getImuSwapXY());
            else if (compass_menu_selection == 1) sensors.setImuInvX(!sensors.getImuInvX());
            else if (compass_menu_selection == 2) sensors.setImuInvY(!sensors.getImuInvY());
            else if (compass_menu_selection == 3) sensors.setImuInvZ(!sensors.getImuInvZ());
            else if (compass_menu_selection == 4) sensors.calibrateAccel();
            else if (compass_menu_selection == 5) sensors.zeroLevel();
            needs_redraw = true;
        } else if (sel_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavBack();
            motion_state = MotionState::PAGE_DATA;
            needs_redraw = true;
        }
    }
}

void UICore::handleAudioInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);

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
    } else if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (audio_menu_selection == 0) {
            soundManager.setMasterSwitch(!soundManager.isMasterSwitchOn());
        } else if (audio_menu_selection == 1) {
            int s = (int)soundManager.getStyle();
            s = (s + 1) % 4;
            soundManager.setStyle((SoundStyle)s);
        }
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        current_state = UIState::MAIN_MENU;
        needs_redraw = true;
    }
}
