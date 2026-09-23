#include "max30102_manager.h"
#include "weather.h"
#include "display.h"
#include "ui_core.h"
#include "sensors.h"
#include "air_mouse.h"
#include "button_manager.h"
#include "hw_config.h"
#include "led_manager.h"
#include "sound_manager.h"
#include "settings_data.h"
#include "wifi_portal.h"
#include "clock.h"
#include "ir_engine.h"
#include "driver/rtc_io.h"
#include "timekeeping.h"

UICore ui;
String UICore::pending_selected_ssid = "";

UICore::UICore() :
    current_state(UIState::APP_HOME),
    imu_subapp(Imu6500SubApp::SUBAPP_MENU),
    imu_subapp_selection(0),
    last_activity_time(millis()),
    display_off(false),
    display_off_time(0),
    just_woke_display(false),
    return_state(UIState::APP_HOME),
    bme_page(0),
    weather_page(0),
    health_page(0),
    health_history_graph_idx(0),
    menu_selection(0),
    menu_scroll_offset(0),
    edit_value(5),
    led_menu_selection(0),
    led_menu_offset(0),
    clock_submenu(ClockSubmenu::MAIN),
    clock_selection(0),
    clock_scroll_offset(0),
    alarm_edit_idx(0),
    alarm_edit_field(0),
    timer_preset_idx(2),
    widgets_selection(0),
    widgets_scroll_offset(0),
    sound_submenu(SoundSubmenu::MAIN),
    sound_selection(0),
    sound_scroll_offset(0),
    ir_submenu(IrSubmenu::MAIN),
    ir_selection(0),
    ir_scroll_offset(0),
    settings_submenu(SettingsSubmenu::MAIN),
    settings_selection(0),
    settings_scroll_offset(0),
    toast_end_time(0),
    kb_callback(nullptr),
    compass_state(CompassState::PAGE_MAIN),
    compass_menu_selection(0),
    compass_menu_offset(0),
    motion_state(MotionState::PAGE_LEVEL),
    needs_redraw(true),
    fm_current_path("/"),
    fm_selection(0),
    fm_scroll_offset(0),
    fm_entry_count(0),
    fm_entries(nullptr) {
    toast_msg[0] = '\0';
}

void UICore::setIrActiveRemotePath(const String& path) {
    irEngine.parseIrFile(path, ir_active_remote);
    ir_selection = 0;
    ir_scroll_offset = 0;
    ir_submenu = IrSubmenu::REMOTE_VIEW;
    needs_redraw = true;
}

void UICore::begin() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        Preferences sched_prefs;
        sched_prefs.begin("sched", false);
        uint32_t now = qclock.getEpoch();
        if (now == 0) now = millis() / 1000;

        uint32_t last_bme_epoch = sched_prefs.getUInt("last_bme", 0);
        uint32_t last_health_epoch = sched_prefs.getUInt("last_health", 0);

        SettingsData& s = settingsManager.get();
        uint32_t intervals_sec[] = {300, 600, 900, 1800, 3600};
        uint32_t bme_interval_sec = intervals_sec[s.bme_interval_idx];
        uint32_t health_interval_sec = intervals_sec[s.health_interval_idx];

        bool bme_due = (last_bme_epoch == 0) || (now >= last_bme_epoch + bme_interval_sec);
        bool health_due = s.health_bg_enabled && ((last_health_epoch == 0) || (now >= last_health_epoch + health_interval_sec));

        if (bme_due) {
            sensors.begin();
            sensors.logBmeSample();
            sched_prefs.putUInt("last_bme", now);
            last_bme_epoch = now;
        }

        if (health_due) {
            max30102Manager.begin();
            if (max30102Manager.takeSampleAndSave(7000)) {
                sched_prefs.putUInt("last_health", now);
                last_health_epoch = now;
            }
        }

        uint32_t next_bme_in = (last_bme_epoch + bme_interval_sec > now) ? (last_bme_epoch + bme_interval_sec - now) : bme_interval_sec;
        if (next_bme_in == 0) next_bme_in = bme_interval_sec;

        uint32_t next_health_in = s.health_bg_enabled ? ((last_health_epoch + health_interval_sec > now) ? (last_health_epoch + health_interval_sec - now) : health_interval_sec) : 0xFFFFFFFF;
        if (next_health_in == 0) next_health_in = health_interval_sec;

        uint32_t sleep_sec = min(next_bme_in, next_health_in);
        sched_prefs.end();

        esp_sleep_enable_timer_wakeup((uint64_t)sleep_sec * 1000000ULL);

        rtc_gpio_pullup_en((gpio_num_t)BTN_CANCEL);
        rtc_gpio_pulldown_dis((gpio_num_t)BTN_CANCEL);
        uint64_t wake_mask = (1ULL << BTN_CANCEL);

        if (s.raise_to_wake) {
            sensors.enableMotionInterruptForSleep();
            rtc_gpio_pullup_en((gpio_num_t)MPU_INT);
            rtc_gpio_pulldown_dis((gpio_num_t)MPU_INT);
            wake_mask |= (1ULL << MPU_INT);
        }

        esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);

        esp_deep_sleep_start();
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
    if (current_state == UIState::APP_IR && ir_submenu == IrSubmenu::TV_B_GONE) {
        if (irEngine.isTvBGoneRunning()) {
            needs_redraw = true;
        }
    }

    if (current_state == UIState::APP_IR && (ir_submenu == IrSubmenu::IR_READ_WAIT || ir_submenu == IrSubmenu::QUICK_REMOTE_WAIT)) {
        if (irEngine.checkCapturedSignal(ir_captured_btn)) {
            soundManager.playNavSelect();
            if (ir_submenu == IrSubmenu::IR_READ_WAIT) {
                ir_submenu = IrSubmenu::IR_READ_RESULT;
            } else {
                irEngine.appendButtonToIrFile("/ir/" + ir_quick_remote_name + ".ir", ir_captured_btn);
                showToast("[BUTTON ADDED]", 1200);
                setIrActiveRemotePath("/ir/" + ir_quick_remote_name + ".ir");
                ir_submenu = IrSubmenu::QUICK_REMOTE_BUILD;
            }
            needs_redraw = true;
        }
    }

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
            return;
        }
        last_activity_time = millis();
    }

    SettingsData& s = settingsManager.get();

    if (!display_off && s.display_timeout_idx < 4) {
        uint32_t disp_timeouts_ms[] = {10000, 30000, 60000, 300000};
        if (millis() - last_activity_time >= disp_timeouts_ms[s.display_timeout_idx]) {
            display_off = true;
            displayManager.setPowerSave(true);
            display_off_time = millis();
        }
    }

    if (display_off) {
        if (s.raise_to_wake) {
            OrientationData o = sensors.getOrientation();
            // Wrist raise detection: typical watch viewing angle
            if (o.pitch >= 15.0f && o.pitch <= 65.0f && fabsf(o.roll) <= 35.0f) {
                display_off = false;
                displayManager.setPowerSave(false);
                last_activity_time = millis();
                needs_redraw = true;
                return;
            }
        }

        // 10 seconds after display is turned off, enter deep sleep
        if (millis() - display_off_time >= 10000) {
            enterDeepSleep();
            return;
        }
    }

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

    if (cancel_evt == BTN_EVT_SHORT_PRESS) {
        if (current_state == UIState::APP_HOME) {
            display_off = !display_off;
            displayManager.setPowerSave(display_off);
            if (display_off) {
                display_off_time = millis();
            }
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_IR) {
            soundManager.playNavBack();
            if (irEngine.isCarrierTestActive()) irEngine.stopCarrierTest();
            if (irEngine.isCapturing()) irEngine.stopCapture();
            if (irEngine.isTvBGoneRunning()) irEngine.stopTvBGone();

            if (ir_submenu == IrSubmenu::MAIN) {
                current_state = UIState::MAIN_MENU;
                menu_selection = 6;
                menu_scroll_offset = 4;
            } else {
                ir_submenu = IrSubmenu::MAIN;
                ir_selection = 0;
                ir_scroll_offset = 0;
            }
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_AUDIO) {
            soundManager.playNavBack();
            if (sound_submenu == SoundSubmenu::MAIN) {
                current_state = UIState::MAIN_MENU;
                menu_selection = 7;
                menu_scroll_offset = 5;
            } else {
                sound_submenu = SoundSubmenu::MAIN;
                sound_selection = 0;
                sound_scroll_offset = 0;
            }
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_SETTINGS) {
            soundManager.playNavBack();
            if (settings_submenu == SettingsSubmenu::MAIN) {
                current_state = UIState::MAIN_MENU;
                menu_selection = 12;
                menu_scroll_offset = 10;
            } else {
                settings_submenu = SettingsSubmenu::MAIN;
                settings_selection = 0;
                settings_scroll_offset = 0;
            }
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_ALTIMETER) {
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 8;
            menu_scroll_offset = 6;
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_LED) {
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 10;
            menu_scroll_offset = 8;
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_WEATHER) {
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 2;
            menu_scroll_offset = 0;
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_HEALTH) {
            soundManager.playNavBack();
            max30102Manager.disableSensor();
            current_state = UIState::MAIN_MENU;
            menu_selection = 4;
            menu_scroll_offset = 2;
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_BATTERY) {
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 9;
            menu_scroll_offset = 7;
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_COMPASS) {
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 3;
            menu_scroll_offset = 1;
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_CLOCK) {
            soundManager.playNavBack();
            if (clock_submenu == ClockSubmenu::MAIN) {
                current_state = UIState::MAIN_MENU;
                menu_selection = 1;
                menu_scroll_offset = 0;
            } else {
                clock_submenu = ClockSubmenu::MAIN;
                clock_selection = 0;
                clock_scroll_offset = 0;
            }
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_ABOUT) {
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 13;
            menu_scroll_offset = 11;
            needs_redraw = true;
            return;
        } else {
            soundManager.playNavBack();
            if (current_state == UIState::MAIN_MENU) {
                current_state = UIState::APP_HOME;
            } else {
                current_state = UIState::MAIN_MENU;
            }
            needs_redraw = true;
            return;
        }
    } else if (cancel_evt == BTN_EVT_LONG_PRESS || (ok_evt == BTN_EVT_LONG_PRESS && current_state != UIState::APP_HOME)) {
        soundManager.playNavBack();
        if (current_state == UIState::APP_IR) {
            if (irEngine.isCarrierTestActive()) irEngine.stopCarrierTest();
            if (irEngine.isCapturing()) irEngine.stopCapture();
            if (irEngine.isTvBGoneRunning()) irEngine.stopTvBGone();
            current_state = UIState::MAIN_MENU;
        } else if (current_state == UIState::APP_AUDIO) {
            soundManager.stop();
            current_state = UIState::MAIN_MENU;
        } else if (current_state == UIState::APP_CLOCK) {
            clock_submenu = ClockSubmenu::MAIN;
            clock_selection = 0;
            clock_scroll_offset = 0;
            current_state = UIState::APP_HOME;
        } else {
            current_state = UIState::APP_HOME;
        }
        needs_redraw = true;
        return;
    }

    switch (current_state) {
        case UIState::APP_HOME: handleHomeInput(); break;
        case UIState::MAIN_MENU: handleMainMenuInput(); break;
        case UIState::APP_CLOCK: handleClockInput(); break;
        case UIState::APP_WEATHER: handleWeatherInput(); break;
        case UIState::APP_SETTINGS: handleSettingsMenuInput(); break;
        case UIState::APP_IR: handleIRInput(); break;
        case UIState::APP_AUDIO: handleSoundInput(); break;
        case UIState::APP_ALTIMETER: handleBmeInput(); break;
        case UIState::APP_LED: handleLedInput(); break;
        case UIState::APP_FILE_MANAGER: handleFileManagerInput(); break;
        case UIState::APP_STORAGE_INFO: handleStorageInfoInput(); break;
        case UIState::APP_KEYBOARD: handleKeyboardInput(); break;
        case UIState::VALUE_EDIT: handleValueEditInput(); break;
        case UIState::APP_COMPASS: handleCompassInput(); break;
        case UIState::APP_HEALTH: handleHealthInput(); break;
        case UIState::APP_MOTION: handleMotionInput(); break;
        default: handleGenericAppInput(); break;
    }
}

void UICore::handleIRInput() {
    switch (ir_submenu) {
        case IrSubmenu::MAIN: handleIrMainInput(); break;
        case IrSubmenu::TV_B_GONE: handleTvBGoneInput(); break;
        case IrSubmenu::CUSTOM_IR: handleIrCustomInput(); break;
        case IrSubmenu::REMOTE_VIEW: handleIrRemoteViewInput(); break;
        case IrSubmenu::IR_READ:
        case IrSubmenu::IR_READ_WAIT:
        case IrSubmenu::IR_READ_RESULT: handleIrReadInput(); break;
        case IrSubmenu::QUICK_REMOTE:
        case IrSubmenu::QUICK_REMOTE_BUILD:
        case IrSubmenu::QUICK_REMOTE_WAIT: handleQuickRemoteInput(); break;
        case IrSubmenu::UNIVERSAL: handleIrUniversalInput(); break;
        case IrSubmenu::RECENT: handleIrRecentInput(); break;
        case IrSubmenu::FAVORITES: handleIrFavoritesInput(); break;
        case IrSubmenu::IR_FILES: handleIrFilesInput(); break;
        case IrSubmenu::IR_LAB: handleIrLabInput(); break;
    }
}

void UICore::handleIrMainInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (ir_selection > 0) {
            ir_selection--;
            if (ir_selection < ir_scroll_offset) ir_scroll_offset = ir_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (ir_selection < IR_MAIN_ITEM_COUNT - 1) {
            ir_selection++;
            if (ir_selection >= ir_scroll_offset + 3) ir_scroll_offset = ir_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        switch (ir_selection) {
            case 0: ir_submenu = IrSubmenu::TV_B_GONE; ir_selection = 0; break;
            case 1: ir_submenu = IrSubmenu::CUSTOM_IR; ir_selection = 0; ir_scroll_offset = 0; break;
            case 2: ir_submenu = IrSubmenu::IR_READ; ir_selection = 0; ir_scroll_offset = 0; break;
            case 3: ir_submenu = IrSubmenu::QUICK_REMOTE; ir_selection = 0; ir_scroll_offset = 0; break;
            case 4: ir_submenu = IrSubmenu::UNIVERSAL; ir_selection = 0; ir_scroll_offset = 0; break;
            case 5: ir_submenu = IrSubmenu::RECENT; ir_selection = 0; ir_scroll_offset = 0; break;
            case 6: ir_submenu = IrSubmenu::FAVORITES; ir_selection = 0; ir_scroll_offset = 0; break;
            case 7:
                ir_file_list = irEngine.listIrFiles();
                ir_submenu = IrSubmenu::IR_FILES;
                ir_selection = 0;
                ir_scroll_offset = 0;
                break;
            case 8: ir_submenu = IrSubmenu::IR_LAB; ir_selection = 0; ir_scroll_offset = 0; break;
        }
        needs_redraw = true;
    }
}

void UICore::handleTvBGoneInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (irEngine.isTvBGoneRunning()) {
            irEngine.stopTvBGone();
            showToast("[TV-B-GONE STOP]", 1200);
        } else {
            irEngine.startTvBGone();
            showToast("[TV-B-GONE START]", 1200);
        }
        needs_redraw = true;
    }
}

void UICore::handleIrCustomInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (ir_selection > 0) {
            ir_selection--;
            if (ir_selection < ir_scroll_offset) ir_scroll_offset = ir_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (ir_selection < IR_CUSTOM_ITEM_COUNT - 1) {
            ir_selection++;
            if (ir_selection >= ir_scroll_offset + 3) ir_scroll_offset = ir_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (ir_selection == 0) {
            ir_file_list = irEngine.listIrFiles();
            ir_submenu = IrSubmenu::IR_FILES;
            ir_selection = 0;
            ir_scroll_offset = 0;
        } else if (ir_selection == 1) {
            ir_submenu = IrSubmenu::RECENT;
            ir_selection = 0;
            ir_scroll_offset = 0;
        } else if (ir_selection == 2) {
            ir_submenu = IrSubmenu::FAVORITES;
            ir_selection = 0;
            ir_scroll_offset = 0;
        } else {
            showToast("[SEARCH READY]", 1200);
        }
        needs_redraw = true;
    }
}

void UICore::handleIrRemoteViewInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    int btn_count = ir_active_remote.buttons.size();
    if (btn_count == 0) return;

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (ir_selection > 0) {
            ir_selection--;
            if (ir_selection < ir_scroll_offset) ir_scroll_offset = ir_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (ir_selection < btn_count - 1) {
            ir_selection++;
            if (ir_selection >= ir_scroll_offset + 3) ir_scroll_offset = ir_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        const IrButton& btn = ir_active_remote.buttons[ir_selection];
        soundManager.playNavSelect();
        bool sent = irEngine.sendButton(btn);
        if (sent) {
            irEngine.addRecent(ir_active_remote.name, btn);
            showToast(("[TX " + btn.name + "]").c_str(), 1000);
        } else {
            showToast("[TX FAIL]", 1000);
        }
        needs_redraw = true;
    }
}

void UICore::handleIrReadInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (ir_submenu == IrSubmenu::IR_READ) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            irEngine.startCapture();
            ir_submenu = IrSubmenu::IR_READ_WAIT;
            needs_redraw = true;
        }
    } else if (ir_submenu == IrSubmenu::IR_READ_RESULT) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            irEngine.sendButton(ir_captured_btn);
            showToast("[TEST TX]", 1000);
            needs_redraw = true;
        }
    }
}

static String pending_qr_remote_name = "";

static void onQuickButtonNameEntered(bool success, const String& btn_name) {
    if (success && btn_name.length() > 0) {
        ui.setIrQuickButtonName(btn_name);
        irEngine.startCapture();
        ui.setIrSubmenu(IrSubmenu::QUICK_REMOTE_WAIT);
        ui.showToast("[WAITING SIGNAL]", 1200);
    }
}

static void onQuickRemoteNameEntered(bool success, const String& rem_name) {
    if (success && rem_name.length() > 0) {
        pending_qr_remote_name = rem_name;
        ui.setIrQuickRemoteName(rem_name);
        ui.setIrActiveRemotePath("/ir/" + rem_name + ".ir");
        ui.showToast("[REMOTE CREATED]", 1200);
        ui.setIrSubmenu(IrSubmenu::QUICK_REMOTE_BUILD);
    }
}

void UICore::handleQuickRemoteInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (ir_submenu == IrSubmenu::QUICK_REMOTE) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            openKeyboard("MyRemote", "Remote Name", KeyboardMode::ALPHA, false, 24, onQuickRemoteNameEntered);
        }
    } else if (ir_submenu == IrSubmenu::QUICK_REMOTE_BUILD) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            openKeyboard("Power", "Button Name", KeyboardMode::ALPHA, false, 24, onQuickButtonNameEntered);
        }
    }
}

void UICore::handleIrUniversalInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        showToast("[UNI SEARCH]", 1200);
    }
}

void UICore::handleIrRecentInput() {
    std::vector<String> list = irEngine.getRecentList();
    int count = list.size();

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (count == 0) return;

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (ir_selection > 0) {
            ir_selection--;
            if (ir_selection < ir_scroll_offset) ir_scroll_offset = ir_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (ir_selection < count - 1) {
            ir_selection++;
            if (ir_selection >= ir_scroll_offset + 3) ir_scroll_offset = ir_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        showToast("[RECENT TX]", 1000);
        needs_redraw = true;
    }
}

void UICore::handleIrFavoritesInput() {
    std::vector<String> list = irEngine.getFavoritesList();
    int count = list.size();

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (count == 0) return;

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (ir_selection > 0) {
            ir_selection--;
            if (ir_selection < ir_scroll_offset) ir_scroll_offset = ir_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (ir_selection < count - 1) {
            ir_selection++;
            if (ir_selection >= ir_scroll_offset + 3) ir_scroll_offset = ir_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        showToast("[FAV TX]", 1000);
        needs_redraw = true;
    }
}

void UICore::handleIrFilesInput() {
    int count = ir_file_list.size();

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (count == 0) return;

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (ir_selection > 0) {
            ir_selection--;
            if (ir_selection < ir_scroll_offset) ir_scroll_offset = ir_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (ir_selection < count - 1) {
            ir_selection++;
            if (ir_selection >= ir_scroll_offset + 3) ir_scroll_offset = ir_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        setIrActiveRemotePath(ir_file_list[ir_selection]);
        needs_redraw = true;
    }
}

void UICore::handleIrLabInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (ir_selection > 0) {
            ir_selection--;
            if (ir_selection < ir_scroll_offset) ir_scroll_offset = ir_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (ir_selection < IR_LAB_ITEM_COUNT - 1) {
            ir_selection++;
            if (ir_selection >= ir_scroll_offset + 3) ir_scroll_offset = ir_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (ir_selection == 0) {
            if (irEngine.isCarrierTestActive()) irEngine.stopCarrierTest();
            else irEngine.startCarrierTest(38000);
        } else if (ir_selection == 1) {
            if (irEngine.isCarrierTestActive()) irEngine.stopCarrierTest();
            else irEngine.startCarrierTest(36000);
        } else if (ir_selection == 2) {
            if (irEngine.isCarrierTestActive()) irEngine.stopCarrierTest();
            else irEngine.startCarrierTest(40000);
        } else {
            showToast("[LAB OK]", 1200);
        }
        needs_redraw = true;
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
    if (menu_selection < 0) menu_selection = 0;
    if (menu_selection < menu_scroll_offset) menu_scroll_offset = menu_selection;
    soundManager.playNavMove();
    needs_redraw = true;
}

void UICore::processNavDown() {
    int max_items = (current_state == UIState::MAIN_MENU) ? MAIN_MENU_ITEM_COUNT : SETTINGS_MAIN_ITEM_COUNT;
    menu_selection++;
    if (menu_selection >= max_items) menu_selection = max_items - 1;
    if (menu_selection >= menu_scroll_offset + 3) menu_scroll_offset = menu_selection - 2;
    soundManager.playNavMove();
    needs_redraw = true;
}

void UICore::handleHomeInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        SettingsData& s = settingsManager.get();
        s.watch_face_style = (s.watch_face_style + WATCH_FACE_COUNT - 1) % WATCH_FACE_COUNT;
        settingsManager.save();
        needs_redraw = true;
        return;
    }

    if (dn_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        SettingsData& s = settingsManager.get();
        s.watch_face_style = (s.watch_face_style + 1) % WATCH_FACE_COUNT;
        settingsManager.save();
        needs_redraw = true;
        return;
    }

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        current_state = UIState::MAIN_MENU;
        menu_selection = 0;
        menu_scroll_offset = 0;
        needs_redraw = true;
    }
}

void UICore::handleClockInput() {
    switch (clock_submenu) {
        case ClockSubmenu::MAIN:         handleClockMenuInput(); break;
        case ClockSubmenu::FACE_SELECT:  handleFaceSelectInput(); break;
        case ClockSubmenu::FACE_WIDGETS: handleFaceWidgetsInput(); break;
        case ClockSubmenu::STOPWATCH:    handleStopwatchInput(); break;
        case ClockSubmenu::TIMER:        handleTimerInput(); break;
        case ClockSubmenu::ALARMS:       handleAlarmsInput(); break;
        case ClockSubmenu::ALARM_EDIT:   handleAlarmEditInput(); break;
        case ClockSubmenu::WORLD_CLOCK:  handleWorldClockInput(); break;
        case ClockSubmenu::PEDOMETER:    handlePedometerInput(); break;
    }
}

void UICore::handleClockMenuInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (clock_selection > 0) {
            clock_selection--;
            if (clock_selection < clock_scroll_offset) clock_scroll_offset = clock_selection;
            needs_redraw = true;
        }
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (clock_selection < CLOCK_MENU_ITEM_COUNT - 1) {
            clock_selection++;
            if (clock_selection >= clock_scroll_offset + 3) clock_scroll_offset = clock_selection - 2;
            needs_redraw = true;
        }
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        switch (clock_selection) {
            case 0: // WATCH FACE
                clock_submenu = ClockSubmenu::FACE_SELECT;
                clock_selection = settingsManager.get().watch_face_style;
                clock_scroll_offset = 0;
                break;
            case 1: // FACE WIDGETS
                clock_submenu = ClockSubmenu::FACE_WIDGETS;
                widgets_selection = 0;
                widgets_scroll_offset = 0;
                break;
            case 2: // STOPWATCH
                clock_submenu = ClockSubmenu::STOPWATCH;
                break;
            case 3: // TIMER
                clock_submenu = ClockSubmenu::TIMER;
                break;
            case 4: // ALARMS
                clock_submenu = ClockSubmenu::ALARMS;
                alarm_edit_idx = 0;
                break;
            case 5: // HOURLY CHIME
                {
                    SettingsData& s = settingsManager.get();
                    s.hourly_chime_enabled = !s.hourly_chime_enabled;
                    settingsManager.save();
                }
                break;
            case 6: // WORLD CLOCK
                clock_submenu = ClockSubmenu::WORLD_CLOCK;
                break;
            case 7: // PEDOMETER
                clock_submenu = ClockSubmenu::PEDOMETER;
                break;
        }
        needs_redraw = true;
    }
}

void UICore::handleFaceSelectInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (clock_selection > 0) {
            clock_selection--;
            if (clock_selection < clock_scroll_offset) clock_scroll_offset = clock_selection;
            needs_redraw = true;
        }
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (clock_selection < WATCH_FACE_COUNT - 1) {
            clock_selection++;
            if (clock_selection >= clock_scroll_offset + 3) clock_scroll_offset = clock_selection - 2;
            needs_redraw = true;
        }
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        settingsManager.get().watch_face_style = clock_selection;
        settingsManager.save();
        clock_submenu = ClockSubmenu::MAIN;
        clock_selection = 0;
        clock_scroll_offset = 0;
        needs_redraw = true;
    }
}

void UICore::handleFaceWidgetsInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (widgets_selection > 0) {
            widgets_selection--;
            if (widgets_selection < widgets_scroll_offset) widgets_scroll_offset = widgets_selection;
            needs_redraw = true;
        }
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (widgets_selection < WIDGETS_ITEM_COUNT - 1) {
            widgets_selection++;
            if (widgets_selection >= widgets_scroll_offset + 3) widgets_scroll_offset = widgets_selection - 2;
            needs_redraw = true;
        }
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        switch (widgets_selection) {
            case 0: s.show_date = !s.show_date; break;
            case 1: s.show_battery = !s.show_battery; break;
            case 2: s.show_weather_widget = !s.show_weather_widget; break;
            case 3: s.show_steps_widget = !s.show_steps_widget; break;
            case 4: s.show_status_icons = !s.show_status_icons; break;
        }
        settingsManager.save();
        needs_redraw = true;
    }
}

void UICore::handleStopwatchInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (!timekeeping.stopwatch.isRunning()) {
            timekeeping.stopwatch.start();
        } else {
            timekeeping.stopwatch.lap();
        }
        needs_redraw = true;
        return;
    }

    if (up_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        if (timekeeping.stopwatch.isRunning()) {
            timekeeping.stopwatch.pause();
        } else if (timekeeping.stopwatch.isPaused()) {
            timekeeping.stopwatch.resume();
        }
        needs_redraw = true;
        return;
    }

    if (dn_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        if (timekeeping.stopwatch.isPaused() || !timekeeping.stopwatch.isRunning()) {
            timekeeping.stopwatch.reset();
        } else {
            timekeeping.stopwatch.pause();
        }
        needs_redraw = true;
        return;
    }
}

void UICore::handleTimerInput() {
    static const uint32_t PRESET_SECS[] = {60, 180, 300, 600, 900, 1800, 3600};
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);

    if (timekeeping.timer.isExpired()) {
        if (ok_evt != BTN_EVT_NONE || up_evt != BTN_EVT_NONE || dn_evt != BTN_EVT_NONE) {
            timekeeping.timer.clearExpired();
            timekeeping.timer.reset();
            soundManager.playNavSelect();
            needs_redraw = true;
            return;
        }
    }

    if (!timekeeping.timer.isRunning() && !timekeeping.timer.isPaused()) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            soundManager.playNavMove();
            if (timer_preset_idx > 0) {
                timer_preset_idx--;
                timekeeping.timer.setDuration(PRESET_SECS[timer_preset_idx]);
                needs_redraw = true;
            }
            return;
        }
        if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            soundManager.playNavMove();
            if (timer_preset_idx < 6) {
                timer_preset_idx++;
                timekeeping.timer.setDuration(PRESET_SECS[timer_preset_idx]);
                needs_redraw = true;
            }
            return;
        }
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            timekeeping.timer.setDuration(PRESET_SECS[timer_preset_idx]);
            timekeeping.timer.start();
            needs_redraw = true;
            return;
        }
    } else if (timekeeping.timer.isRunning()) {
        if (ok_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            timekeeping.timer.pause();
            needs_redraw = true;
            return;
        }
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavMove();
            timekeeping.timer.reset();
            needs_redraw = true;
            return;
        }
    } else if (timekeeping.timer.isPaused()) {
        if (ok_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            timekeeping.timer.resume();
            needs_redraw = true;
            return;
        }
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavMove();
            timekeeping.timer.reset();
            needs_redraw = true;
            return;
        }
    }
}

void UICore::handleAlarmsInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);

    if (timekeeping.alarmManager.isRinging()) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            timekeeping.alarmManager.snooze(timekeeping.alarmManager.getRingingIdx());
            soundManager.playNavSelect();
            needs_redraw = true;
            return;
        }
        if (dn_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_SHORT_PRESS) {
            timekeeping.alarmManager.dismiss(timekeeping.alarmManager.getRingingIdx());
            soundManager.playNavBack();
            needs_redraw = true;
            return;
        }
    }

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        alarm_edit_idx = (alarm_edit_idx + 2) % 3;
        needs_redraw = true;
        return;
    }

    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        alarm_edit_idx = (alarm_edit_idx + 1) % 3;
        needs_redraw = true;
        return;
    }

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        timekeeping.alarmManager.toggleAlarm(alarm_edit_idx);
        needs_redraw = true;
        return;
    }

    if (ok_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavSelect();
        clock_submenu = ClockSubmenu::ALARM_EDIT;
        alarm_edit_field = 0;
        needs_redraw = true;
        return;
    }
}

void UICore::handleAlarmEditInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);

    AlarmEntry& a = timekeeping.alarmManager.getAlarm(alarm_edit_idx);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (alarm_edit_field == 0) {
            a.hour = (a.hour + 1) % 24;
        } else {
            a.minute = (a.minute + 1) % 60;
        }
        needs_redraw = true;
        return;
    }

    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (alarm_edit_field == 0) {
            a.hour = (a.hour + 23) % 24;
        } else {
            a.minute = (a.minute + 59) % 60;
        }
        needs_redraw = true;
        return;
    }

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (alarm_edit_field == 0) {
            alarm_edit_field = 1;
        } else {
            a.enabled = true;
            timekeeping.alarmManager.save();
            clock_submenu = ClockSubmenu::ALARMS;
        }
        needs_redraw = true;
        return;
    }
}

void UICore::handleWorldClockInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    SettingsData& s = settingsManager.get();

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        s.world_clock_tz_idx = (s.world_clock_tz_idx + 11) % 12;
        settingsManager.save();
        needs_redraw = true;
        return;
    }

    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        s.world_clock_tz_idx = (s.world_clock_tz_idx + 1) % 12;
        settingsManager.save();
        needs_redraw = true;
        return;
    }

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        s.world_clock_tz_idx = (s.world_clock_tz_idx + 1) % 12;
        settingsManager.save();
        needs_redraw = true;
        return;
    }
}

void UICore::handlePedometerInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    SettingsData& s = settingsManager.get();

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (s.step_goal <= 49000) {
            s.step_goal += 1000;
            settingsManager.save();
            needs_redraw = true;
        }
        return;
    }

    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        soundManager.playNavMove();
        if (s.step_goal >= 2000) {
            s.step_goal -= 1000;
            settingsManager.save();
            needs_redraw = true;
        }
        return;
    }

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        timekeeping.pedometer.reset();
        needs_redraw = true;
        return;
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
            case 1:
                current_state = UIState::APP_CLOCK;
                clock_submenu = ClockSubmenu::MAIN;
                clock_selection = 0;
                clock_scroll_offset = 0;
                break;
            case 2: current_state = UIState::APP_WEATHER; weather_page = 0; break;
            case 3: current_state = UIState::APP_COMPASS; compass_state = CompassState::PAGE_MAIN; break;
            case 4: current_state = UIState::APP_HEALTH; health_page = 0; max30102Manager.enableSensor(); break;
            case 5: current_state = UIState::APP_MOTION; imu_subapp = Imu6500SubApp::SUBAPP_MENU; imu_subapp_selection = 0; break;
            case 6: current_state = UIState::APP_IR; ir_submenu = IrSubmenu::MAIN; ir_selection = 0; ir_scroll_offset = 0; break;
            case 7: current_state = UIState::APP_AUDIO; sound_submenu = SoundSubmenu::MAIN; sound_selection = 0; sound_scroll_offset = 0; break;
            case 8: current_state = UIState::APP_ALTIMETER; bme_page = 0; break;
            case 9: current_state = UIState::APP_BATTERY; break;
            case 10: current_state = UIState::APP_LED; led_menu_selection = 0; led_menu_offset = 0; break;
            case 11: current_state = UIState::APP_FILE_MANAGER; fm_current_path = "/"; loadDirectory("/"); break;
            case 12:
                current_state = UIState::APP_SETTINGS;
                settings_submenu = SettingsSubmenu::MAIN;
                settings_selection = 0;
                settings_scroll_offset = 0;
                break;
            case 13: current_state = UIState::APP_ABOUT; break;
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
        case SettingsSubmenu::HEALTH_SETTINGS: handleHealthSettingsInput(); break;
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
    if (wifiPortal.isScanning()) return;
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
        if (s.wifi_enabled) wifiPortal.enableWifi();
        else wifiPortal.disableWifi();
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
            s.raise_to_wake = !s.raise_to_wake;
            settingsManager.save();
        } else if (settings_selection == 3) {
            s.wifi_auto_off_idx = (s.wifi_auto_off_idx + 1) % WIFI_AUTO_OFF_COUNT;
            settingsManager.save();
        } else if (settings_selection == 4) {
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
            settings_submenu = SettingsSubmenu::HEALTH_SETTINGS;
            settings_selection = 0;
            settings_scroll_offset = 0;
        } else if (settings_selection == 3) {
            showToast("[STATUS: OK]", 1500);
        }
        needs_redraw = true;
    }
}

void UICore::handleHealthSettingsInput() {
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
        if (settings_selection >= HEALTH_SETTINGS_ITEM_COUNT) settings_selection = HEALTH_SETTINGS_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 3) settings_scroll_offset = settings_selection - 2;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) {
            s.health_bg_enabled = !s.health_bg_enabled;
            settingsManager.save();
        } else if (settings_selection == 1) {
            s.health_interval_idx = (s.health_interval_idx + 1) % HEALTH_INTERVAL_COUNT;
            settingsManager.save();
        }
        needs_redraw = true;
    }
}

void UICore::handleHealthInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (health_page == 0) {
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavMove();
            health_page = 1;
            max30102Manager.disableSensor();
            needs_redraw = true;
        }
    } else {
        if (up_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavMove();
            health_page = 0;
            max30102Manager.enableSensor();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            health_history_graph_idx = (health_history_graph_idx + 1) % 3;
            needs_redraw = true;
        }
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
    ButtonEvent cancel_evt = btnManager.getEvent(BTN_ID_CANCEL);

    if (ok_evt == BTN_EVT_SHORT_PRESS || ok_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playAlert();
        showToast("[RESETTING...]", 1500);
        settingsManager.get() = SettingsData();
        settingsManager.save();
        sensors.factoryResetCalibration();
        delay(800);
        ESP.restart();
    } else if (cancel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavBack();
        showToast("[CANCELLED]", 1000);
        settings_submenu = SettingsSubmenu::SYSTEM;
        settings_selection = 1;
        settings_scroll_offset = 0;
        needs_redraw = true;
    }
}

void UICore::handleBmeInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (bme_page == 4) {
        // Calibration & Diagnostic page
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            float off = sensors.getTempOffset() + 0.5f;
            if (off > 10.0f) off = 10.0f;
            sensors.setTempOffset(off);
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            float off = sensors.getTempOffset() - 0.5f;
            if (off < -10.0f) off = -10.0f;
            sensors.setTempOffset(off);
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (up_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavMove();
            bme_page = 3;
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavMove();
            bme_page = 0;
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            sensors.resetBmeCalibration();
            soundManager.playNavSelect();
            showToast("[CAL RESET]", 1200);
            needs_redraw = true;
        }
    } else {
        // Pages 0 (Pressure), 1 (Humidity), 2 (Temperature), 3 (Altitude)
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            bme_page = (bme_page > 0) ? bme_page - 1 : 4;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            bme_page = (bme_page < 4) ? bme_page + 1 : 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (bme_page == 3) {
                // Altitude page: toggle measuring/paused or set zero
                if (sensors.getHeightState() == BmeHeightState::OFF) {
                    sensors.zeroAltitude();
                    sensors.toggleHeightMeasurement();
                    showToast("[ZERO SET / RUN]", 1200);
                } else {
                    sensors.toggleHeightMeasurement();
                    if (sensors.getHeightState() == BmeHeightState::PAUSED) {
                        showToast("[PAUSED]", 1200);
                    } else {
                        showToast("[MEASURING]", 1200);
                    }
                }
            } else {
                // Log sample
                sensors.logBmeSample();
                showToast("[SAMPLE LOGGED]", 1200);
            }
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_LONG_PRESS && bme_page == 3) {
            sensors.zeroAltitude();
            soundManager.playNavSelect();
            showToast("[ALT ZEROED]", 1200);
            needs_redraw = true;
        }
    }
}

void UICore::handleLedInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (led_menu_selection > 0) {
            led_menu_selection--;
            if (led_menu_selection < led_menu_offset) led_menu_offset = led_menu_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (led_menu_selection < LED_MENU_ITEM_COUNT - 1) {
            led_menu_selection++;
            if (led_menu_selection >= led_menu_offset + 3) led_menu_offset = led_menu_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        switch (led_menu_selection) {
            case 0: // Master Sw
                ledManager.setMasterSwitch(!ledManager.isMasterSwitchOn());
                showToast(ledManager.isMasterSwitchOn() ? "[LED ON]" : "[LED OFF]", 1000);
                break;
            case 1: { // Mode
                LedMode cur = ledManager.getMode();
                LedMode next;
                if (cur == LedMode::OFF) next = LedMode::SOLID;
                else if (cur == LedMode::SOLID) next = LedMode::BREATHING;
                else if (cur == LedMode::BREATHING) next = LedMode::RAINBOW;
                else if (cur == LedMode::RAINBOW) next = LedMode::COMPASS_SYNC;
                else next = LedMode::OFF;
                ledManager.setMode(next);
                break;
            }
            case 2: { // Brightness
                uint8_t b = ledManager.getBrightness();
                if (b < 64) b = 64;
                else if (b < 128) b = 128;
                else if (b < 192) b = 192;
                else if (b < 255) b = 255;
                else b = 32;
                ledManager.setBrightness(b);
                break;
            }
            case 3: { // Presets
                static int preset_idx = 0;
                preset_idx = (preset_idx + 1) % LedManager::LED_PRESET_COUNT;
                ledManager.setColor(ledManager.preset_colors[preset_idx]);
                if (ledManager.getMode() == LedMode::OFF) ledManager.setMode(LedMode::SOLID);
                showToast("[COLOR CHANGED]", 1000);
                break;
            }
            case 4: // Effects
                ledManager.triggerPulse(CRGB::White, 3, 80);
                showToast("[BURST EFFECT]", 1000);
                break;
            case 5: // Factory Rst
                ledManager.setMasterSwitch(true);
                ledManager.setBrightness(50);
                ledManager.setColor(CRGB::Blue);
                ledManager.setMode(LedMode::SOLID);
                showToast("[LED RESET]", 1200);
                break;
        }
        needs_redraw = true;
    }
}

void UICore::handleWeatherInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_SHORT_PRESS) {
        weather_page = (weather_page == 0) ? 1 : 0;
        soundManager.playNavMove();
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        weather.forceUpdate();
        soundManager.playNavSelect();
        showToast("[SYNCING OWM...]", 1500);
        needs_redraw = true;
    }
}

void UICore::handleGenericAppInput() {
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
            } else {
                String filename = fm_entries[fm_selection].name;
                if (filename.endsWith(".ir")) {
                    String fullPath = fm_current_path;
                    if (!fullPath.endsWith("/")) fullPath += "/";
                    fullPath += filename;
                    current_state = UIState::APP_IR;
                    setIrActiveRemotePath(fullPath);
                }
            }
        }
        needs_redraw = true;
    }
}

void UICore::handleStorageInfoInput() {
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
    delay(100);

    SettingsData& sleep_s = settingsManager.get();

    rtc_gpio_pullup_en((gpio_num_t)BTN_CANCEL);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_CANCEL);
    uint64_t wake_mask = (1ULL << BTN_CANCEL);

    if (sleep_s.raise_to_wake) {
        sensors.enableMotionInterruptForSleep();
        rtc_gpio_pullup_en((gpio_num_t)MPU_INT);
        rtc_gpio_pulldown_dis((gpio_num_t)MPU_INT);
        wake_mask |= (1ULL << MPU_INT);
    } else {
        sensors.clearMpuInterrupt();
    }

    esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);

    Preferences sched_prefs;
    sched_prefs.begin("sched", false);
    uint32_t now = qclock.getEpoch();
    if (now == 0) now = millis() / 1000;

    uint32_t last_bme_epoch = sched_prefs.getUInt("last_bme", 0);
    uint32_t last_health_epoch = sched_prefs.getUInt("last_health", 0);
    uint32_t sleep_intervals_sec[] = {300, 600, 900, 1800, 3600};
    uint32_t bme_sec = sleep_intervals_sec[sleep_s.bme_interval_idx];
    uint32_t health_sec = sleep_intervals_sec[sleep_s.health_interval_idx];

    uint32_t next_bme_in = (last_bme_epoch + bme_sec > now) ? (last_bme_epoch + bme_sec - now) : bme_sec;
    if (next_bme_in == 0) next_bme_in = bme_sec;

    uint32_t next_health_in = sleep_s.health_bg_enabled ? ((last_health_epoch + health_sec > now) ? (last_health_epoch + health_sec - now) : health_sec) : 0xFFFFFFFF;
    if (next_health_in == 0) next_health_in = health_sec;

    uint32_t sleep_sec = min(next_bme_in, next_health_in);
    sched_prefs.end();

    esp_sleep_enable_timer_wakeup((uint64_t)sleep_sec * 1000000ULL);

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
    } else if (compass_state == CompassState::PAGE_METRICS) {
        if (up_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_MAIN;
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
    } else if (compass_state == CompassState::PAGE_CAL_MENU) {
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
    } else if (compass_state == CompassState::CAL_SWEEP) {
        if (sensors.getCalState() == MagCalState::RESULT) {
            compass_state = CompassState::CAL_RESULT;
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            sensors.cancelMagCalibration();
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
        needs_redraw = true;
    } else if (compass_state == CompassState::CAL_RESULT) {
        if (up_evt == BTN_EVT_SHORT_PRESS) {
            sensors.saveCurrentCalibration();
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
            sensors.cancelMagCalibration();
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
    } else if (compass_state == CompassState::CAL_TELEMETRY) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        }
        needs_redraw = true;
    } else if (compass_state == CompassState::CAL_DECLINATION) {
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
    if (imu_subapp == Imu6500SubApp::SUBAPP_MENU) {
        ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
        ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
        ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            imu_subapp_selection--;
            if (imu_subapp_selection < 0) imu_subapp_selection = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            imu_subapp_selection++;
            if (imu_subapp_selection >= IMU_SUBAPP_COUNT) imu_subapp_selection = IMU_SUBAPP_COUNT - 1;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (imu_subapp_selection == 0) {
                imu_subapp = Imu6500SubApp::SUBAPP_ALTIMETER;
                motion_state = MotionState::PAGE_LEVEL;
            } else {
                imu_subapp = Imu6500SubApp::SUBAPP_AIRMOUSE;
            }
            needs_redraw = true;
        }
        return;
    }

    if (imu_subapp == Imu6500SubApp::SUBAPP_AIRMOUSE) {
        handleAirMouseInput();
        return;
    }

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (motion_state == MotionState::PAGE_LEVEL) {
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            motion_state = MotionState::PAGE_DATA;
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (sensors.isBmeOk()) sensors.zeroAltitude();
            else showToast("[BME UNLINKED]", 1500);
            needs_redraw = true;
        }
    } else if (motion_state == MotionState::PAGE_DATA) {
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            motion_state = MotionState::PAGE_SETTINGS;
            compass_menu_selection = 0;
            needs_redraw = true;
        } else if (up_evt == BTN_EVT_SHORT_PRESS) {
            motion_state = MotionState::PAGE_LEVEL;
            needs_redraw = true;
        }
    } else if (motion_state == MotionState::PAGE_SETTINGS) {
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
            if (compass_menu_selection == 0) {
                if (sensors.isBmeOk()) sensors.zeroAltitude();
                else showToast("[BME UNLINKED]", 1500);
            } else if (compass_menu_selection == 1) sensors.zeroLevel();
            else if (compass_menu_selection == 2) sensors.calibrateAccel();
            else if (compass_menu_selection == 3) sensors.setImuSwapXY(!sensors.getImuSwapXY());
            else if (compass_menu_selection == 4) sensors.setImuInvX(!sensors.getImuInvX());
            else if (compass_menu_selection == 5) sensors.setImuInvY(!sensors.getImuInvY());
            else if (compass_menu_selection == 6) sensors.setImuInvZ(!sensors.getImuInvZ());
            needs_redraw = true;
        }
    }
}

void UICore::handleAirMouseInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavSelect();
        airMouse.cycleSensitivityUp();
        showToast("[SENS INCREASED]", 1000);
        needs_redraw = true;
        return;
    }

    if (dn_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavSelect();
        airMouse.cycleSensitivityDown();
        showToast("[SENS DECREASED]", 1000);
        needs_redraw = true;
        return;
    }

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (!airMouse.isEnabled() || airMouse.getBleStatus() == AirMouseBleStatus::DISCONNECTED) {
            if (airMouse.isEnabled()) airMouse.stop();
            airMouse.start();
        } else {
            airMouse.toggleMovement();
        }
        needs_redraw = true;
        return;
    }

    if (airMouse.getMode() == AirMouseMode::POINTER) {
        if (up_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            airMouse.clickLeft();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            airMouse.clickRight();
            needs_redraw = true;
        }
    } else {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            soundManager.playNavMove();
            airMouse.scrollUp();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            soundManager.playNavMove();
            airMouse.scrollDown();
            needs_redraw = true;
        }
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
