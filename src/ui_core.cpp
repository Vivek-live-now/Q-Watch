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
#include "qapp_loader.h"
#include "anim_engine.h"
#include "wireless_recon.h"
#include "power_manager.h"
#include "battery.h"
#include "vibration_manager.h"

UICore ui;
String UICore::pending_selected_ssid = "";

UICore::UICore() :
    current_state(UIState::APP_HOME),
    imu_subapp(Imu6500SubApp::SUBAPP_MENU),
    imu_subapp_selection(0),
    mouse_settings_selection(0),
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
    cal_orient_preset(0),
    imu_orient_preset(0),
    motion_state(MotionState::PAGE_LEVEL),
    needs_redraw(true),
    fm_current_path("/"),
    fm_selection(0),
    fm_scroll_offset(0),
    fm_entry_count(0),
    fm_entries(nullptr),
    app_count(0),
    app_selection(0),
    app_scroll_offset(0),
    anim_count(0),
    anim_selection(0),
    anim_scroll_offset(0),
    anim_hud_visible(false),
    anim_hud_timer(0),
    anim_return_state(UIState::APP_ANIM_LIST),
    recon_submenu(ReconSubmenu::MAIN),
    recon_selection(0),
    recon_scroll_offset(0),
    ble_list_selection(0),
    ble_list_scroll_offset(0),
    last_radar_tick_time(0),
    radar_sweep_angle(0.0f),
    creator_count(0),
    creator_cursor(0),
    creator_edit_field(0),
    metronome_bpm(120),
    metronome_active(false),
    metronome_beat(0),
    last_metronome_tick(0),
    lab_freq(2700),
    lab_duty(50),
    lab_sweep_active(false),
    lab_sweep_freq(2000),
    last_sweep_time(0) {
    toast_msg[0] = '\0';
}

void UICore::setIrActiveRemotePath(const String& path) {
    if (ir_file_list.empty()) {
        ir_file_list = irEngine.listIrFiles();
    }
    irEngine.parseIrFile(path, ir_active_remote);
    ir_selection = 0;
    ir_scroll_offset = 0;
    ir_submenu = IrSubmenu::REMOTE_VIEW;
    needs_redraw = true;
}

uint32_t UICore::calculateNextRecordIntervalSec() {
    SettingsData& s = settingsManager.get();
    if (!s.auto_record_enabled) {
        return 0; // Disabled: zero periodic timer wakeups
    }
    uint32_t intervals_sec[] = {300, 600, 900, 1800, 3600};
    int idx = s.auto_record_interval_idx;
    if (idx < 0 || idx >= 5) idx = 0;
    return intervals_sec[idx];
}

void UICore::performSilentBackgroundRecording() {
    SettingsData& s = settingsManager.get();
    if (!s.auto_record_enabled) return;

    if (s.auto_record_target_idx == 0 || s.auto_record_target_idx == 1) {
        sensors.logBmeSample();
    }
    if (s.auto_record_target_idx == 0 || s.auto_record_target_idx == 2) {
        max30102Manager.takeSampleAndSave(6000);
    }
}

void UICore::performSilentDeepSleepWake() {
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    SettingsData& s = settingsManager.get();
    if (s.auto_record_enabled) {
        sensors.begin();
        max30102Manager.begin();
        performSilentBackgroundRecording();
    }

    // Check if user pressed any button during background sampling
    if (digitalRead(BTN_CANCEL) == LOW || digitalRead(BTN_OK) == LOW ||
        digitalRead(BTN_UP) == LOW || digitalRead(BTN_DN) == LOW) {
        return; // User interacted! Abort deep sleep and boot normally
    }

    uint32_t sleep_sec = calculateNextRecordIntervalSec();
    if (sleep_sec > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)sleep_sec * 1000000ULL);
    } else {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    }

    rtc_gpio_pullup_en((gpio_num_t)BTN_CANCEL);
    rtc_gpio_pulldown_dis((gpio_num_t)BTN_CANCEL);
    uint64_t wake_mask = (1ULL << BTN_CANCEL);

    if (s.raise_to_wake) {
        sensors.enableMotionInterruptForSleep();
        rtc_gpio_pullup_en((gpio_num_t)MPU_INT);
        rtc_gpio_pulldown_dis((gpio_num_t)MPU_INT);
        wake_mask |= (1ULL << MPU_INT);
    } else {
        sensors.clearMpuInterrupt();
    }

    esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
#endif
}

void UICore::begin() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        performSilentDeepSleepWake();
        return;
    }
    powerManager.begin();
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

    if (current_state == UIState::APP_RUNNING) {
        if (!qappLoader.isRunning()) {
            current_state = UIState::APP_APPS;
            needs_redraw = true;
        } else {
            static uint32_t last_app_tick = 0;
            uint32_t now = millis();
            float dt = (now - last_app_tick) / 1000.0f;
            if (dt > 0.1f) dt = 0.1f;
            if (dt < 0.001f) dt = 0.001f;
            last_app_tick = now;
            qappLoader.update(dt);
            needs_redraw = true;
        }
    }

    if (current_state == UIState::APP_ANIM_PLAYER) {
        if (animEngine.update()) {
            needs_redraw = true;
        }
        if (anim_hud_visible && millis() - anim_hud_timer > 2000) {
            anim_hud_visible = false;
            needs_redraw = true;
        }
    }

    if (current_state == UIState::APP_MOCHI) {
        CalibratedSensorData cal = sensors.getCalData();
        OrientationData ori = sensors.getOrientation();
        mochiPet.updatePhysics(ori.pitch, ori.roll,
                               cal.ax, cal.ay, cal.az,
                               cal.gx, cal.gy, cal.gz);
        mochiPet.update(0.033f);
        needs_redraw = true;
    }

    if (current_state == UIState::APP_WIRELESS) {
        wirelessRecon.loop();
        radar_sweep_angle += 0.12f;
        if (radar_sweep_angle >= 6.2831853f) radar_sweep_angle -= 6.2831853f;

        if (recon_submenu == ReconSubmenu::BLE_RADAR && wirelessRecon.hasTargetLock()) {
            const BleTarget* tgt = wirelessRecon.getLockedTarget();
            if (tgt) {
                uint16_t interval = wirelessRecon.getGeigerTickInterval(tgt->rssi);
                if (interval > 0 && (millis() - last_radar_tick_time >= interval)) {
                    last_radar_tick_time = millis();
                    soundManager.playTone(2800, 12);
                }
            }
        } else if (recon_submenu == ReconSubmenu::DEAUTH_DETECT) {
            if (wirelessRecon.isAttackDetected()) {
                static uint32_t last_alarm = 0;
                if (millis() - last_alarm > 600) {
                    last_alarm = millis();
                    soundManager.playTone(3200, 40);
                }
            }
        }
        needs_redraw = true;
    }

    if (current_state == UIState::APP_IR && (ir_submenu == IrSubmenu::IR_READ_WAIT || ir_submenu == IrSubmenu::QUICK_REMOTE_WAIT)) {
        if (irEngine.checkCapturedSignal(ir_captured_btn)) {
            soundManager.playNavSelect();
            irEngine.stopCapture();
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

    bool any_button = btnManager.hasAnyEvent();

    if (any_button) {
        if (display_off || current_state == UIState::SLEEPING) {
            display_off = false;
            displayManager.setPowerSave(false);
            if (current_state == UIState::SLEEPING) {
                current_state = UIState::APP_HOME;
            }
            btnManager.flushEvents();
            last_activity_time = millis();
            display_off_time = 0;
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

    if (display_off || current_state == UIState::SLEEPING) {
        if (s.raise_to_wake) {
            OrientationData o = sensors.getOrientation();
            // Wrist raise detection: typical watch viewing angle
            if (o.pitch >= 15.0f && o.pitch <= 65.0f && fabsf(o.roll) <= 35.0f) {
                display_off = false;
                displayManager.setPowerSave(false);
                if (current_state == UIState::SLEEPING) {
                    current_state = UIState::APP_HOME;
                }
                last_activity_time = millis();
                display_off_time = 0;
                needs_redraw = true;
                return;
            }
        }

        // 10 seconds after display is turned off, enter deep sleep
        if (display_off && display_off_time > 0 && millis() - display_off_time >= 10000) {
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

    if (current_state != UIState::APP_RUNNING && current_state != UIState::APP_KEYBOARD && !(current_state == UIState::APP_MOTION && imu_subapp == Imu6500SubApp::SUBAPP_AIRMOUSE)) {
        ButtonEvent cancel_evt = btnManager.peekEvent(BTN_ID_CANCEL);
        ButtonEvent ok_evt = btnManager.peekEvent(BTN_ID_OK);

        if (cancel_evt == BTN_EVT_SHORT_PRESS) {
            btnManager.getEvent(BTN_ID_CANCEL);
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
            } else if (ir_submenu == IrSubmenu::REMOTE_VIEW) {
                ir_submenu = IrSubmenu::IR_FILES;
                ir_selection = 0;
                ir_scroll_offset = 0;
            } else if (ir_submenu == IrSubmenu::IR_FILES || ir_submenu == IrSubmenu::RECENT || ir_submenu == IrSubmenu::FAVORITES) {
                ir_submenu = IrSubmenu::CUSTOM_IR;
                ir_selection = 0;
                ir_scroll_offset = 0;
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
                menu_selection = 17;
                menu_scroll_offset = 14;
            } else if (settings_submenu == SettingsSubmenu::WIFI_DETAILS) {
                settings_submenu = SettingsSubmenu::CONNECTIVITY;
                settings_selection = 0;
                settings_scroll_offset = 0;
            } else if (settings_submenu == SettingsSubmenu::WIFI_SCAN) {
                settings_submenu = SettingsSubmenu::CONNECTIVITY;
                settings_selection = 1;
                settings_scroll_offset = 0;
            } else if (settings_submenu == SettingsSubmenu::FILE_SERVER_DETAILS) {
                settings_submenu = SettingsSubmenu::CONNECTIVITY;
                settings_selection = 3;
                settings_scroll_offset = 1;
            } else if (settings_submenu == SettingsSubmenu::TIME_SYNC_STATUS) {
                settings_submenu = SettingsSubmenu::TIME;
                settings_selection = 1;
                settings_scroll_offset = 0;
            } else if (settings_submenu == SettingsSubmenu::RESET_CONFIRM) {
                settings_submenu = SettingsSubmenu::SYSTEM;
                settings_selection = 1;
                settings_scroll_offset = 0;
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
            if (battery_page > 0) {
                battery_page = 0;
                battery_menu_selection = 0;
                battery_menu_scroll_offset = 0;
            } else {
                current_state = UIState::MAIN_MENU;
                menu_selection = 9;
                menu_scroll_offset = 7;
            }
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_COMPASS) {
            soundManager.playNavBack();
            if (compass_state == CompassState::CAL_ORIENTATION_3D) {
                sensors.revertMagOrientation();
                compass_state = CompassState::PAGE_CAL_MENU;
            } else if (compass_state == CompassState::CAL_SWEEP ||
                       compass_state == CompassState::CAL_RESULT ||
                       compass_state == CompassState::CAL_TELEMETRY ||
                       compass_state == CompassState::CAL_DECLINATION) {
                compass_state = CompassState::PAGE_CAL_MENU;
            } else if (compass_state == CompassState::PAGE_CAL_MENU) {
                compass_state = CompassState::PAGE_METRICS;
            } else if (compass_state == CompassState::PAGE_METRICS) {
                compass_state = CompassState::PAGE_MAIN;
            } else {
                current_state = UIState::MAIN_MENU;
                menu_selection = 3;
                menu_scroll_offset = 1;
            }
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_MOTION) {
            soundManager.playNavBack();
            if (motion_state == MotionState::PAGE_ORIENTATION_3D) {
                sensors.revertImuOrientation();
                motion_state = MotionState::PAGE_SETTINGS;
            } else if (motion_state == MotionState::PAGE_SETTINGS) {
                motion_state = MotionState::PAGE_DATA;
            } else if (motion_state == MotionState::PAGE_DATA) {
                motion_state = MotionState::PAGE_LEVEL;
            } else if (imu_subapp != Imu6500SubApp::SUBAPP_MENU) {
                if (imu_subapp == Imu6500SubApp::SUBAPP_AIRMOUSE) {
                    airMouse.stop();
                }
                imu_subapp = Imu6500SubApp::SUBAPP_MENU;
            } else {
                current_state = UIState::MAIN_MENU;
                menu_selection = 5;
                menu_scroll_offset = 3;
            }
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
            menu_selection = 18;
            menu_scroll_offset = 15;
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_MOCHI) {
            soundManager.playNavBack();
            if (mochiPet.getSubmode() != MochiSubmode::INTERACTIVE) {
                mochiPet.setSubmode(MochiSubmode::INTERACTIVE);
            } else {
                current_state = UIState::MAIN_MENU;
                menu_selection = 14;
                menu_scroll_offset = 11;
            }
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_ANIM_LIST) {
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 13;
            menu_scroll_offset = 11;
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_ANIM_PLAYER) {
            animEngine.close();
            soundManager.playNavBack();
            current_state = anim_return_state;
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_WIRELESS) {
            soundManager.playNavBack();
            if (recon_submenu == ReconSubmenu::MAIN) {
                current_state = UIState::MAIN_MENU;
                menu_selection = 15;
                menu_scroll_offset = 12;
            } else if (recon_submenu == ReconSubmenu::BLE_RADAR) {
                recon_submenu = ReconSubmenu::BLE_LIST;
            } else {
                if (recon_submenu == ReconSubmenu::BLE_LIST) {
                    wirelessRecon.stopBleScan();
                } else if (recon_submenu == ReconSubmenu::DEAUTH_DETECT) {
                    wirelessRecon.stopDeauthMonitor();
                } else if (recon_submenu == ReconSubmenu::PKT_MONITOR) {
                    wirelessRecon.stopPacketMonitor();
                }
                recon_submenu = ReconSubmenu::MAIN;
            }
            needs_redraw = true;
            return;
        } else if (current_state == UIState::APP_VIBRATION) {
            soundManager.playNavBack();
            vibrationManager.stop();
            current_state = UIState::MAIN_MENU;
            menu_selection = 16;
            menu_scroll_offset = 13;
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
        } else if (cancel_evt == BTN_EVT_LONG_PRESS || (ok_evt == BTN_EVT_LONG_PRESS && current_state != UIState::APP_HOME && current_state != UIState::APP_ANIM_LIST && current_state != UIState::APP_ANIM_PLAYER && current_state != UIState::APP_MOCHI && !(current_state == UIState::APP_AUDIO && (sound_submenu == SoundSubmenu::COMPOSER || sound_submenu == SoundSubmenu::CREATOR_EDIT)))) {
            if (cancel_evt == BTN_EVT_LONG_PRESS) {
                btnManager.getEvent(BTN_ID_CANCEL);
            } else {
                btnManager.getEvent(BTN_ID_OK);
            }
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
            } else if (current_state == UIState::APP_ANIM_PLAYER) {
                animEngine.close();
                current_state = UIState::MAIN_MENU;
            } else if (current_state == UIState::APP_ANIM_LIST) {
                current_state = UIState::MAIN_MENU;
            } else if (current_state == UIState::APP_WIRELESS) {
                wirelessRecon.stopBleScan();
                wirelessRecon.stopDeauthMonitor();
                wirelessRecon.stopPacketMonitor();
                recon_submenu = ReconSubmenu::MAIN;
                current_state = UIState::MAIN_MENU;
            } else if (current_state == UIState::APP_MOTION) {
                airMouse.stop();
                current_state = UIState::MAIN_MENU;
            } else if (current_state == UIState::APP_VIBRATION) {
                vibrationManager.stop();
                current_state = UIState::MAIN_MENU;
            } else {
                current_state = UIState::APP_HOME;
            }
            needs_redraw = true;
            return;
        }
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
        case UIState::APP_APPS: handleAppsInput(); break;
        case UIState::APP_RUNNING: handleAppRunningInput(); break;
        case UIState::APP_ANIM_LIST: handleAnimListInput(); break;
        case UIState::APP_ANIM_PLAYER: handleAnimPlayerInput(); break;
        case UIState::APP_WIRELESS: handleWirelessInput(); break;
        case UIState::APP_MOCHI: handleMochiInput(); break;
        case UIState::APP_VIBRATION: handleVibrationInput(); break;
        case UIState::APP_STORAGE_INFO: handleStorageInfoInput(); break;
        case UIState::APP_KEYBOARD: handleKeyboardInput(); break;
        case UIState::VALUE_EDIT: handleValueEditInput(); break;
        case UIState::APP_COMPASS: handleCompassInput(); break;
        case UIState::APP_HEALTH: handleHealthInput(); break;
        case UIState::APP_MOTION: handleMotionInput(); break;
        case UIState::APP_BATTERY: handleBatteryInput(); break;
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
            if (ir_selection >= ir_scroll_offset + 4) ir_scroll_offset = ir_selection - 3;
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
            if (ir_selection >= ir_scroll_offset + 4) ir_scroll_offset = ir_selection - 3;
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
            if (ir_selection >= ir_scroll_offset + 4) ir_scroll_offset = ir_selection - 3;
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
            if (ir_selection >= ir_scroll_offset + 4) ir_scroll_offset = ir_selection - 3;
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
            if (ir_selection >= ir_scroll_offset + 4) ir_scroll_offset = ir_selection - 3;
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
            if (ir_selection >= ir_scroll_offset + 4) ir_scroll_offset = ir_selection - 3;
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
            if (ir_selection >= ir_scroll_offset + 4) ir_scroll_offset = ir_selection - 3;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (ir_selection == 0) {
            if (irEngine.isCarrierTestActive() && irEngine.getCarrierFreq() == 38000) {
                irEngine.stopCarrierTest();
                irEngine.setLastLabStatus("CARRIER OFF");
                showToast("[CARRIER 38k OFF]", 1000);
            } else {
                if (irEngine.isCarrierTestActive()) irEngine.stopCarrierTest();
                irEngine.startCarrierTest(38000);
                irEngine.setLastLabStatus("CARRIER 38k ON");
                showToast("[CARRIER 38k ON]", 1000);
            }
        } else if (ir_selection == 1) {
            if (irEngine.isCarrierTestActive() && irEngine.getCarrierFreq() == 36000) {
                irEngine.stopCarrierTest();
                irEngine.setLastLabStatus("CARRIER OFF");
                showToast("[CARRIER 36k OFF]", 1000);
            } else {
                if (irEngine.isCarrierTestActive()) irEngine.stopCarrierTest();
                irEngine.startCarrierTest(36000);
                irEngine.setLastLabStatus("CARRIER 36k ON");
                showToast("[CARRIER 36k ON]", 1000);
            }
        } else if (ir_selection == 2) {
            if (irEngine.isCarrierTestActive() && irEngine.getCarrierFreq() == 40000) {
                irEngine.stopCarrierTest();
                irEngine.setLastLabStatus("CARRIER OFF");
                showToast("[CARRIER 40k OFF]", 1000);
            } else {
                if (irEngine.isCarrierTestActive()) irEngine.stopCarrierTest();
                irEngine.startCarrierTest(40000);
                irEngine.setLastLabStatus("CARRIER 40k ON");
                showToast("[CARRIER 40k ON]", 1000);
            }
        } else if (ir_selection == 3) {
            irEngine.pulseLedDc(1500);
            irEngine.setLastLabStatus("DC TORCH OK");
            showToast("[LED DC ON 1.5s]", 1500);
        } else if (ir_selection == 4) {
            String lb_res;
            bool ok = irEngine.runLoopbackTest(lb_res);
            irEngine.setLastLabStatus(lb_res);
            showToast(ok ? "[LB: PASS]" : "[LB: FAIL]", 1500);
        } else if (ir_selection == 5) {
            int8_t off = irEngine.runCalibration(38000);
            char buf[32];
            snprintf(buf, sizeof(buf), "CAL OFFSET: %d us", off);
            irEngine.setLastLabStatus(buf);
            char toast_buf[32];
            snprintf(toast_buf, sizeof(toast_buf), "[OFFSET: %d us]", off);
            showToast(toast_buf, 1500);
        } else if (ir_selection == 6) {
            irEngine.togglePolarity();
            bool inv = irEngine.isPolarityInverted();
            irEngine.setLastLabStatus(inv ? "POLARITY: INVERTED" : "POLARITY: NORMAL");
            showToast(inv ? "[POL: INVERTED]" : "[POL: NORMAL]", 1500);
        } else if (ir_selection == 7) {
            bool ok = irEngine.sendParsed("NIKAI", 0, 0x807F, 24);
            irEngine.setLastLabStatus(ok ? "TX: NIKAI 24b OK" : "TX: NIKAI FAIL");
            showToast(ok ? "[TX NIKAI 24b]" : "[TX FAIL]", 1200);
        } else if (ir_selection == 8) {
            bool ok = irEngine.sendParsed("RCA", 4, 0x0C, 24);
            irEngine.setLastLabStatus(ok ? "TX: RCA 24b OK" : "TX: RCA FAIL");
            showToast(ok ? "[TX RCA 24b]" : "[TX FAIL]", 1200);
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
    if (menu_selection >= menu_scroll_offset + 4) menu_scroll_offset = menu_selection - 3;
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
            if (clock_selection >= clock_scroll_offset + 4) clock_scroll_offset = clock_selection - 3;
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
            if (clock_selection >= clock_scroll_offset + 4) clock_scroll_offset = clock_selection - 3;
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
            if (widgets_selection >= widgets_scroll_offset + 4) widgets_scroll_offset = widgets_selection - 3;
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
        if (menu_selection == 0) {
            soundManager.playNavSelect();
        } else {
            soundManager.playAppLaunch();
            delay(150);
        }
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
            case 9: current_state = UIState::APP_BATTERY; battery_page = 0; battery_menu_selection = 0; battery_menu_scroll_offset = 0; break;
            case 10: current_state = UIState::APP_LED; led_menu_selection = 0; led_menu_offset = 0; break;
            case 11: current_state = UIState::APP_FILE_MANAGER; fm_current_path = "/"; loadDirectory("/"); break;
            case 12: current_state = UIState::APP_APPS; loadAppsList(); break;
            case 13: current_state = UIState::APP_ANIM_LIST; loadAnimList(); break;
            case 14:
                current_state = UIState::APP_MOCHI;
                mochiPet.setSubmode(MochiSubmode::INTERACTIVE);
                break;
            case 15:
                current_state = UIState::APP_WIRELESS;
                recon_submenu = ReconSubmenu::MAIN;
                recon_selection = 0;
                recon_scroll_offset = 0;
                break;
            case 16:
                current_state = UIState::APP_VIBRATION;
                vibe_selection = 0;
                vibe_offset = 0;
                break;
            case 17:
                current_state = UIState::APP_SETTINGS;
                settings_submenu = SettingsSubmenu::MAIN;
                settings_selection = 0;
                settings_scroll_offset = 0;
                break;
            case 18: current_state = UIState::APP_ABOUT; break;
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
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
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
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (settings_selection == 0) {
            settings_submenu = SettingsSubmenu::WIFI_DETAILS;
            settings_selection = 0;
            settings_scroll_offset = 0;
        } else if (settings_selection == 1) {
            if (wifiPortal.isHotspotActive()) {
                wifiPortal.stopPortal();
                showToast("[HOTSPOT: OFF]", 1500);
            } else {
                wifiPortal.startPortal();
                showToast("[HOTSPOT: ON 192.168.4.1]", 2000);
            }
        } else if (settings_selection == 2) {
            settings_submenu = SettingsSubmenu::WIFI_SCAN;
            settings_selection = 0;
            settings_scroll_offset = 0;
            wifiPortal.startScan();
        } else if (settings_selection == 3) {
            SettingsData& s = settingsManager.get();
            s.ble_enabled = !s.ble_enabled;
            settingsManager.save();
            showToast(s.ble_enabled ? "[BLE: ON]" : "[BLE: OFF]", 1500);
        } else if (settings_selection == 4) {
            settings_submenu = SettingsSubmenu::FILE_SERVER_DETAILS;
            settings_selection = 0;
            settings_scroll_offset = 0;
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
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
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
                settings_selection = 0;
                settings_scroll_offset = 0;
            }
        }
    }
}

void UICore::handleWifiDetailsInput() {
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
        if (settings_selection >= WIFI_DETAILS_ITEM_COUNT) settings_selection = WIFI_DETAILS_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) {
            s.wifi_enabled = !s.wifi_enabled;
            settingsManager.save();
            if (s.wifi_enabled) wifiPortal.enableWifi();
            else wifiPortal.disableWifi();
        } else if (settings_selection == 1) {
            s.wifi_tx_power_idx = (s.wifi_tx_power_idx + 1) % WIFI_TX_POWER_COUNT;
            settingsManager.save();
            wifiPortal.applyTxPower();
            showToast(WIFI_TX_POWER_TOASTS[s.wifi_tx_power_idx], 2000);
        } else if (settings_selection == 2) {
            showToast(wifiPortal.getDetailedStatusStr(), 2000);
        } else if (settings_selection == 3) {
            String ssid = wifiPortal.getSSID();
            showToast(ssid.length() > 0 ? ssid.c_str() : "NO SSID", 2000);
        } else if (settings_selection == 4) {
            String ip = wifiPortal.getIP();
            showToast(ip.length() > 0 ? ip.c_str() : "0.0.0.0", 2000);
        } else if (settings_selection == 5) {
            settings_submenu = SettingsSubmenu::WIFI_SCAN;
            settings_selection = 0;
            settings_scroll_offset = 0;
            wifiPortal.startScan();
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
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
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
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
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
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
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
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
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
        if (settings_selection >= AUTO_RECORD_ITEM_COUNT) settings_selection = AUTO_RECORD_ITEM_COUNT - 1;
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
        soundManager.playNavMove();
        needs_redraw = true;
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        SettingsData& s = settingsManager.get();
        if (settings_selection == 0) {
            s.auto_record_enabled = !s.auto_record_enabled;
            s.health_bg_enabled = s.auto_record_enabled;
            settingsManager.save();
            showToast(s.auto_record_enabled ? "[AUTO REC: ON]" : "[AUTO REC: OFF]", 1200);
        } else if (settings_selection == 1) {
            s.auto_record_interval_idx = (s.auto_record_interval_idx + 1) % AUTO_RECORD_INTERVAL_COUNT;
            s.bme_interval_idx = s.auto_record_interval_idx;
            s.health_interval_idx = s.auto_record_interval_idx;
            settingsManager.save();
        } else if (settings_selection == 2) {
            s.auto_record_target_idx = (s.auto_record_target_idx + 1) % AUTO_RECORD_TARGET_COUNT;
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
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (!max30102Manager.isAvailable()) {
                if (max30102Manager.retryInit()) {
                    showToast("[MAX30102 CONNECTED]", 1200);
                } else {
                    showToast("[PROBE FAILED: NACK]", 1200);
                }
            } else {
                max30102Manager.enableSensor();
                showToast("[RE-INITIALIZING]", 1000);
            }
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
        if (settings_selection >= settings_scroll_offset + 4) settings_scroll_offset = settings_selection - 3;
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
            if (led_menu_selection >= led_menu_offset + 4) led_menu_offset = led_menu_selection - 3;
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
            if (fm_selection >= fm_scroll_offset + 4) fm_scroll_offset = fm_selection - 3;
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
                String lower = filename;
                lower.toLowerCase();
                if (lower.endsWith(".ir")) {
                    String fullPath = fm_current_path;
                    if (!fullPath.endsWith("/")) fullPath += "/";
                    fullPath += filename;
                    current_state = UIState::APP_IR;
                    setIrActiveRemotePath(fullPath);
                } else if (lower.endsWith(".qapp")) {
                    String fullPath = fm_current_path;
                    if (!fullPath.endsWith("/")) fullPath += "/";
                    int lastSlash = filename.lastIndexOf('/');
                    if (lastSlash >= 0) filename = filename.substring(lastSlash + 1);
                    fullPath += filename;
                    soundManager.playAppLaunch();
                    delay(150);
                    QAppErrorCode err = qappLoader.loadApp(fullPath.c_str());
                    if (err == QAPP_OK) {
                        current_state = UIState::APP_RUNNING;
                    } else {
                        soundManager.playAlert();
                        showToast("LOAD FAILED", 1500);
                    }
                } else if (lower.endsWith(".anim") || lower.endsWith(".bmp")) {
                    String fullPath = fm_current_path;
                    if (!fullPath.endsWith("/")) fullPath += "/";
                    fullPath += filename;
                    soundManager.playAppLaunch();
                    delay(150);
                    openAnimationPlayer(fullPath.c_str(), UIState::APP_FILE_MANAGER);
                }
            }
        }
        needs_redraw = true;
    }
}

void UICore::handleStorageInfoInput() {
}

static void ensureAppFileExists(const char* filename, const char* name, const char* ver, const char* author, uint32_t caps, uint32_t psram) {
    String full_path = String("/apps/") + filename;
    if (LittleFS.exists(full_path)) {
        File f_chk = LittleFS.open(full_path, "r");
        if (f_chk) {
            size_t sz = f_chk.size();
            QAppFileHeader test_hdr;
            size_t rb = f_chk.read((uint8_t*)&test_hdr, sizeof(QAppFileHeader));
            f_chk.close();
            if (rb == sizeof(QAppFileHeader) && sz >= sizeof(QAppFileHeader) && test_hdr.magic == QAPP_MAGIC) {
                return;
            }
        }
        LittleFS.remove(full_path);
    }

    QAppFileHeader fhdr;
    memset(&fhdr, 0, sizeof(fhdr));
    fhdr.magic = QAPP_MAGIC;
    fhdr.api_version = QAPP_API_VERSION;
    fhdr.required_caps = caps;
    strncpy(fhdr.name, name, sizeof(fhdr.name) - 1);
    strncpy(fhdr.version, ver, sizeof(fhdr.version) - 1);
    strncpy(fhdr.author, author, sizeof(fhdr.author) - 1);
    fhdr.required_psram = psram;

    uint32_t code_len = 16;
    uint32_t data_len = sizeof(QAppHeader);

    fhdr.code_offset = sizeof(QAppFileHeader);
    fhdr.code_size = code_len;
    fhdr.data_offset = fhdr.code_offset + fhdr.code_size;
    fhdr.data_size = data_len;
    fhdr.bss_size = 64;
    fhdr.reloc_offset = fhdr.data_offset + fhdr.data_size;
    fhdr.reloc_count = 1;
    fhdr.entry_offset = 0;

    File f = LittleFS.open(full_path, "w");
    if (!f) return;

    f.write((const uint8_t*)&fhdr, sizeof(fhdr));

    uint8_t dummy_code[16] = {0};
    f.write(dummy_code, code_len);

    uint8_t dummy_data[sizeof(QAppHeader)] = {0};
    f.write(dummy_data, data_len);

    QAppReloc reloc;
    memset(&reloc, 0, sizeof(reloc));
    reloc.section = 0;
    reloc.type = QRELOC_DATA_ADDR;
    reloc.offset = 4;
    f.write((const uint8_t*)&reloc, sizeof(reloc));

    f.close();
}

static void provisionDefaultAppsIfNeeded() {
    if (!LittleFS.exists("/apps")) {
        LittleFS.mkdir("/apps");
    }
    ensureAppFileExists("tilt_ball.qapp", "Tilt Ball", "1.0.0", "007 Agent",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED, 2048);
    ensureAppFileExists("compass_hud.qapp", "Compass HUD", "1.0.0", "007 Agent",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MAG, 1024);
    ensureAppFileExists("invaders.qapp", "007 Invaders", "1.0.0", "MI6 Cyber",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE, 4096);
    ensureAppFileExists("dice.qapp", "Tactical Dice", "1.0.0", "MI6 Cyber",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED, 2048);
    ensureAppFileExists("snake.qapp", "Retro Snake", "1.0.0", "MI6 Cyber",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE, 2048);
    ensureAppFileExists("f1_race.qapp", "F1 Grand Prix", "1.0.0", "MI6 Cyber",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE, 2048);
    ensureAppFileExists("pacman.qapp", "Pacman Arcade", "1.0.0", "MI6 Cyber",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE, 4096);
    ensureAppFileExists("breakout.qapp", "Breakout 007", "1.0.0", "MI6 Cyber",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE, 2048);
    ensureAppFileExists("space_impact.qapp", "Space Impact 2", "1.0.0", "MI6 Cyber",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE, 2048);
    ensureAppFileExists("bounce.qapp", "Nokia Bounce", "1.0.0", "MI6 Cyber",
                       QAPP_CAP_DISPLAY | QAPP_CAP_BUTTONS | QAPP_CAP_MPU | QAPP_CAP_AUDIO | QAPP_CAP_RGB_LED | QAPP_CAP_STORAGE, 2048);
}

void UICore::loadAppsList() {
    app_count = 0;
    app_selection = 0;
    app_scroll_offset = 0;

    provisionDefaultAppsIfNeeded();

    File dir = LittleFS.open("/apps");
    if (!dir || !dir.isDirectory()) return;

    File file = dir.openNextFile();
    while (file && app_count < MAX_APPS) {
        String fname = file.name();
        int lastSlash = fname.lastIndexOf('/');
        if (lastSlash >= 0) {
            fname = fname.substring(lastSlash + 1);
        }

        if (fname.endsWith(".qapp")) {
            AppEntry& entry = app_entries[app_count];
            entry.filename = fname;
            entry.valid = false;

            QAppHeader hdr;
            String full_path = String("/apps/") + fname;
            if (QAppLoader::inspectFile(full_path.c_str(), &hdr) == QAPP_OK) {
                strncpy(entry.name, hdr.name, sizeof(entry.name) - 1);
                entry.name[sizeof(entry.name) - 1] = '\0';
                strncpy(entry.version, hdr.version, sizeof(entry.version) - 1);
                entry.version[sizeof(entry.version) - 1] = '\0';
                strncpy(entry.author, hdr.author, sizeof(entry.author) - 1);
                entry.author[sizeof(entry.author) - 1] = '\0';
                entry.valid = true;
            } else {
                strncpy(entry.name, fname.c_str(), sizeof(entry.name) - 1);
                entry.name[sizeof(entry.name) - 1] = '\0';
                strcpy(entry.version, "?");
                strcpy(entry.author, "Unknown");
            }
            app_count++;
        }
        file = dir.openNextFile();
    }

    if (app_count == 0) {
        const struct {
            const char* file;
            const char* name;
            const char* ver;
            const char* auth;
        } defaults[] = {
            { "tilt_ball.qapp", "Tilt Ball", "1.0.0", "007 Agent" },
            { "compass_hud.qapp", "Compass HUD", "1.0.0", "007 Agent" },
            { "invaders.qapp", "007 Invaders", "1.0.0", "MI6 Cyber" },
            { "dice.qapp", "Tactical Dice", "1.0.0", "MI6 Cyber" },
            { "snake.qapp", "Retro Snake", "1.0.0", "MI6 Cyber" },
            { "f1_race.qapp", "F1 Grand Prix", "1.0.0", "MI6 Cyber" },
            { "pacman.qapp", "Pacman Arcade", "1.0.0", "MI6 Cyber" },
            { "breakout.qapp", "Breakout 007", "1.0.0", "MI6 Cyber" },
            { "space_impact.qapp", "Space Impact 2", "1.0.0", "MI6 Cyber" },
            { "bounce.qapp", "Nokia Bounce", "1.0.0", "MI6 Cyber" }
        };
        for (size_t i = 0; i < sizeof(defaults)/sizeof(defaults[0]) && app_count < MAX_APPS; i++) {
            AppEntry& entry = app_entries[app_count];
            entry.filename = defaults[i].file;
            strncpy(entry.name, defaults[i].name, sizeof(entry.name) - 1);
            entry.name[sizeof(entry.name) - 1] = '\0';
            strncpy(entry.version, defaults[i].ver, sizeof(entry.version) - 1);
            entry.version[sizeof(entry.version) - 1] = '\0';
            strncpy(entry.author, defaults[i].auth, sizeof(entry.author) - 1);
            entry.author[sizeof(entry.author) - 1] = '\0';
            entry.valid = true;
            app_count++;
        }
    }
}

void UICore::handleAppsInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (app_selection > 0) {
            app_selection--;
            if (app_selection < app_scroll_offset) {
                app_scroll_offset = app_selection;
            }
            soundManager.playNavMove();
            needs_redraw = true;
        }
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (app_selection < app_count - 1) {
            app_selection++;
            if (app_selection >= app_scroll_offset + 4) {
                app_scroll_offset = app_selection - 3;
            }
            soundManager.playNavMove();
            needs_redraw = true;
        }
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        if (app_count > 0 && app_selection < app_count) {
            String full_path = String("/apps/") + app_entries[app_selection].filename;
            soundManager.playAppLaunch();
            delay(150);
            QAppErrorCode err = qappLoader.loadApp(full_path.c_str());
            if (err == QAPP_OK) {
                current_state = UIState::APP_RUNNING;
            } else {
                soundManager.playAlert();
                showToast("LOAD FAILED", 1500);
            }
            needs_redraw = true;
        }
    }

    ButtonEvent cancel_evt = btnManager.getEvent(BTN_ID_CANCEL);
    if (cancel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavBack();
        current_state = UIState::MAIN_MENU;
        menu_selection = 12; // APPS
        if (menu_selection >= menu_scroll_offset + 4) {
            menu_scroll_offset = menu_selection - 3;
        }
        needs_redraw = true;
    }
}

void UICore::handleAppRunningInput() {
    ButtonEvent cancel_evt = btnManager.getEvent(BTN_ID_CANCEL);
    if (cancel_evt == BTN_EVT_LONG_PRESS) {
        qappLoader.unloadApp();
        current_state = UIState::APP_APPS;
        soundManager.playNavBack();
        needs_redraw = true;
        return;
    }

    for (int b = 0; b < BTN_COUNT; b++) {
        ButtonEvent evt = btnManager.getEvent((ButtonID)b);
        if (evt != BTN_EVT_NONE) {
            uint8_t qbtn = 0;
            if (b == BTN_ID_UP) qbtn = QBTN_UP;
            else if (b == BTN_ID_OK) qbtn = QBTN_OK;
            else if (b == BTN_ID_DN) qbtn = QBTN_DOWN;
            else if (b == BTN_ID_CANCEL) qbtn = QBTN_CANCEL;

            QButtonEvent qevt = QEVT_BTN_SHORT_CLICK;
            if (evt == BTN_EVT_LONG_PRESS) qevt = QEVT_BTN_LONG_HOLD;

            qappLoader.handleButton(qbtn, qevt);
        }
    }
}

void UICore::loadAnimList() {
    anim_count = 0;
    anim_selection = 0;
    anim_scroll_offset = 0;

    if (!LittleFS.exists("/anim")) {
        LittleFS.mkdir("/anim");
    }

    auto scanDir = [&](const char* dirpath) {
        if (!LittleFS.exists(dirpath)) return;
        File dir = LittleFS.open(dirpath);
        if (!dir || !dir.isDirectory()) return;

        File file = dir.openNextFile();
        while (file && anim_count < MAX_ANIM_ENTRIES) {
            String fname = file.name();
            if (fname.startsWith(dirpath)) {
                fname = fname.substring(strlen(dirpath));
                if (fname.startsWith("/")) fname = fname.substring(1);
            } else if (fname.startsWith("/")) {
                fname = fname.substring(1);
            }

            String lower = fname;
            lower.toLowerCase();
            if (lower.endsWith(".anim") || lower.endsWith(".bmp")) {
                AnimEntry& entry = anim_entries[anim_count];
                entry.filename = fname;
                String fp = String(dirpath);
                if (!fp.endsWith("/")) fp += "/";
                fp += fname;
                entry.fullPath = fp;
                entry.isBmp = lower.endsWith(".bmp");
                entry.frameCount = 1;
                entry.delayMs = 1000;

                if (!entry.isBmp) {
                    AnimHeader hdr;
                    if (file.read((uint8_t*)&hdr, sizeof(hdr)) == sizeof(hdr)) {
                        if (hdr.magic == ANIM_MAGIC) {
                            entry.frameCount = hdr.frame_count;
                            entry.delayMs = hdr.frame_delay_ms;
                        }
                    }
                }
                anim_count++;
            }
            file = dir.openNextFile();
        }
    };

    scanDir("/anim");
    if (anim_count < MAX_ANIM_ENTRIES) {
        scanDir("/boot");
    }
}

void UICore::openAnimationPlayer(const char* filepath, UIState return_state) {
    if (!filepath || strlen(filepath) == 0) return;
    anim_return_state = return_state;
    if (animEngine.open(filepath)) {
        current_state = UIState::APP_ANIM_PLAYER;
        anim_hud_visible = true;
        anim_hud_timer = millis();
        needs_redraw = true;
    } else {
        soundManager.playAlert();
        showToast("OPEN FAILED", 1500);
        needs_redraw = true;
    }
}

void UICore::handleAnimListInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (anim_selection > 0) {
            anim_selection--;
            if (anim_selection < anim_scroll_offset) {
                anim_scroll_offset = anim_selection;
            }
            soundManager.playNavMove();
            needs_redraw = true;
        }
    }

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (anim_selection < anim_count - 1) {
            anim_selection++;
            if (anim_selection >= anim_scroll_offset + 4) {
                anim_scroll_offset = anim_selection - 3;
            }
            soundManager.playNavMove();
            needs_redraw = true;
        }
    }

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        if (anim_count > 0 && anim_selection < anim_count) {
            soundManager.playAppLaunch();
            delay(150);
            openAnimationPlayer(anim_entries[anim_selection].fullPath.c_str(), UIState::APP_ANIM_LIST);
        }
    } else if (ok_evt == BTN_EVT_LONG_PRESS) {
        if (anim_count > 0 && anim_selection < anim_count) {
            if (animEngine.setAsBootAnimation(anim_entries[anim_selection].fullPath.c_str())) {
                soundManager.playNavSelect();
                showToast("SET AS BOOT", 1500);
            } else {
                soundManager.playAlert();
                showToast("SET BOOT FAILED", 1500);
            }
            needs_redraw = true;
        }
    }
}

void UICore::handleAnimPlayerInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt != BTN_EVT_NONE || dn_evt != BTN_EVT_NONE || ok_evt != BTN_EVT_NONE) {
        anim_hud_visible = true;
        anim_hud_timer = millis();
    }

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        animEngine.togglePlayPause();
        needs_redraw = true;
        return;
    }

    if (ok_evt == BTN_EVT_LONG_PRESS) {
        if (animEngine.setAsBootAnimation(animEngine.getFilePath())) {
            soundManager.playNavSelect();
            showToast("SET AS BOOT", 1500);
        } else {
            soundManager.playAlert();
            showToast("SET BOOT FAILED", 1500);
        }
        needs_redraw = true;
        return;
    }

    if (up_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        if (animEngine.isPaused()) {
            animEngine.stepForward();
        } else {
            animEngine.cycleSpeed();
        }
        needs_redraw = true;
        return;
    }

    if (dn_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavMove();
        if (animEngine.isPaused()) {
            animEngine.stepBackward();
        } else {
            animEngine.setLoop(!animEngine.isLooping());
            if (animEngine.isLooping()) {
                showToast("LOOP: ON", 1000);
            } else {
                showToast("LOOP: OFF", 1000);
            }
        }
        needs_redraw = true;
        return;
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

    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        current_state = UIState::APP_SETTINGS;
        needs_redraw = true;
    }
}

void UICore::enterDeepSleep() {
    UIState prev_state = (current_state != UIState::SLEEPING) ? current_state : UIState::APP_HOME;
    current_state = UIState::SLEEPING;
    display_off = true;
    displayManager.setPowerSave(true);
    delay(50);

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

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    esp_sleep_enable_ext1_wakeup(wake_mask, ESP_EXT1_WAKEUP_ANY_LOW);
    gpio_wakeup_enable((gpio_num_t)BTN_OK, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BTN_UP, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BTN_DN, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BTN_CANCEL, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
#endif

    uint32_t sleep_sec = calculateNextRecordIntervalSec();

    // Sleep loop for Light Sleep / Display Off
    while (current_state == UIState::SLEEPING) {
        powerManager.executeSleep(sleep_sec, sleep_s.raise_to_wake, wake_mask);

        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_TIMER) {
            // Background timer fired: record silently without turning on OLED!
            if (sleep_s.auto_record_enabled) {
                performSilentBackgroundRecording();
                sleep_sec = calculateNextRecordIntervalSec();
            } else {
                sleep_sec = 0;
            }
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
            // Check if user pressed any button during background sampling
            if (digitalRead(BTN_CANCEL) == LOW || digitalRead(BTN_OK) == LOW ||
                digitalRead(BTN_UP) == LOW || digitalRead(BTN_DN) == LOW) {
                break;
            }
#endif
            // Timer wake handled silently: continue sleeping!
            continue;
        }

        // Real user wakeup from ext1 (button, MPU motion) or GPIO
        break;
    }

    display_off = false;
    displayManager.setPowerSave(false);
    current_state = (prev_state != UIState::SLEEPING) ? prev_state : UIState::APP_HOME;
    last_activity_time = millis();
    display_off_time = 0;
    btnManager.flushEvents();
    soundManager.playWake();
    needs_redraw = true;
}

void UICore::handleBatteryInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (battery_page == 0) {
        if (dn_evt == BTN_EVT_SHORT_PRESS) {
            battery_page = 1;
            battery_menu_selection = 0;
            battery_menu_scroll_offset = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (up_evt == BTN_EVT_SHORT_PRESS) {
            battery_page = 4;
            battery_menu_selection = 0;
            battery_menu_scroll_offset = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            battery.readPercentage();
            soundManager.playNavSelect();
            showToast("[BATTERY REFRESHED]", 1000);
            needs_redraw = true;
        }
    } else if (battery_page == 1) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            if (battery_menu_selection > 0) {
                battery_menu_selection--;
                if (battery_menu_selection < battery_menu_scroll_offset) {
                    battery_menu_scroll_offset = battery_menu_selection;
                }
                soundManager.playNavMove();
                needs_redraw = true;
            }
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            if (battery_menu_selection < 3) {
                battery_menu_selection++;
                if (battery_menu_selection >= battery_menu_scroll_offset + 4) {
                    battery_menu_scroll_offset = battery_menu_selection - 3;
                }
                soundManager.playNavMove();
                needs_redraw = true;
            }
        } else if (up_evt == BTN_EVT_LONG_PRESS) {
            battery_page = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_LONG_PRESS) {
            battery_page = 2;
            battery_menu_selection = 0;
            battery_menu_scroll_offset = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            powerManager.setProfile(static_cast<PowerProfile>(battery_menu_selection));
            soundManager.playNavSelect();
            showToast("[PROFILE APPLIED]", 1200);
            needs_redraw = true;
        }
    } else if (battery_page == 2) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            if (battery_menu_selection > 0) {
                battery_menu_selection--;
                if (battery_menu_selection < battery_menu_scroll_offset) {
                    battery_menu_scroll_offset = battery_menu_selection;
                }
                soundManager.playNavMove();
                needs_redraw = true;
            }
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            if (battery_menu_selection < 3) {
                battery_menu_selection++;
                if (battery_menu_selection >= battery_menu_scroll_offset + 4) {
                    battery_menu_scroll_offset = battery_menu_selection - 3;
                }
                soundManager.playNavMove();
                needs_redraw = true;
            }
        } else if (up_evt == BTN_EVT_LONG_PRESS) {
            battery_page = 1;
            battery_menu_selection = 0;
            battery_menu_scroll_offset = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_LONG_PRESS) {
            battery_page = 3;
            battery_menu_selection = 0;
            battery_menu_scroll_offset = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            SettingsData& s = settingsManager.get();
            if (battery_menu_selection == 0) {
                int eng = (static_cast<int>(powerManager.getSleepEngine()) + 1) % 4;
                powerManager.setSleepEngine(static_cast<SleepEngine>(eng));
            } else if (battery_menu_selection == 1) {
                powerManager.setPedometer247Enabled(!powerManager.isPedometer247Enabled());
                s.pedometer_247 = powerManager.isPedometer247Enabled();
                settingsManager.save();
            } else if (battery_menu_selection == 2) {
                s.raise_to_wake = !s.raise_to_wake;
                settingsManager.save();
            } else if (battery_menu_selection == 3) {
                s.display_timeout_idx = (s.display_timeout_idx + 1) % DISPLAY_TIMEOUT_COUNT;
                settingsManager.save();
            }
            soundManager.playNavSelect();
            needs_redraw = true;
        }
    } else if (battery_page == 3) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            if (battery_menu_selection > 0) {
                battery_menu_selection--;
                if (battery_menu_selection < battery_menu_scroll_offset) {
                    battery_menu_scroll_offset = battery_menu_selection;
                }
                soundManager.playNavMove();
                needs_redraw = true;
            }
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            if (battery_menu_selection < 3) {
                battery_menu_selection++;
                if (battery_menu_selection >= battery_menu_scroll_offset + 4) {
                    battery_menu_scroll_offset = battery_menu_selection - 3;
                }
                soundManager.playNavMove();
                needs_redraw = true;
            }
        } else if (up_evt == BTN_EVT_LONG_PRESS) {
            battery_page = 2;
            battery_menu_selection = 0;
            battery_menu_scroll_offset = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_LONG_PRESS) {
            battery_page = 4;
            battery_menu_selection = 0;
            battery_menu_scroll_offset = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            SettingsData& s = settingsManager.get();
            if (battery_menu_selection == 0) {
                powerManager.setEcoRadioCutEnabled(!powerManager.isEcoRadioCutEnabled());
                s.eco_radio_cut = powerManager.isEcoRadioCutEnabled();
                settingsManager.save();
            } else if (battery_menu_selection == 1) {
                powerManager.setEcoLedBlockEnabled(!powerManager.isEcoLedBlockEnabled());
                s.eco_led_block = powerManager.isEcoLedBlockEnabled();
                settingsManager.save();
            } else if (battery_menu_selection == 2) {
                powerManager.setEcoAudioMuteEnabled(!powerManager.isEcoAudioMuteEnabled());
                s.eco_audio_mute = powerManager.isEcoAudioMuteEnabled();
                settingsManager.save();
            } else if (battery_menu_selection == 3) {
                showToast("[CUTOFF: 3.20V]", 1000);
            }
            soundManager.playNavSelect();
            needs_redraw = true;
        }
    } else if (battery_page == 4) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_LONG_PRESS) {
            battery_page = 3;
            battery_menu_selection = 0;
            battery_menu_scroll_offset = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_LONG_PRESS) {
            battery_page = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            bool next_state = !powerManager.isUlpEnabled();
            powerManager.setUlpEnabled(next_state);
            settingsManager.save();
            soundManager.playNavSelect();
            showToast(next_state ? "[ULP: ENABLED]" : "[ULP: DISABLED]", 1000);
            needs_redraw = true;
        }
    }
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
            if (compass_menu_selection >= compass_menu_offset + 4) compass_menu_offset = compass_menu_selection - 3;
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (compass_menu_selection == 0) {
                sensors.startMagCalibration();
                compass_state = CompassState::CAL_SWEEP;
            } else if (compass_menu_selection == 1) {
                cal_orient_preset = (sensors.getMagCalibration().invert_z ? 4 : 0) + (sensors.getMagCalibration().orientation_mode % 4);
                sensors.setPreviewMagOrientation(cal_orient_preset % 4, cal_orient_preset >= 4);
                compass_state = CompassState::CAL_ORIENTATION_3D;
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
    } else if (compass_state == CompassState::CAL_ORIENTATION_3D) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            cal_orient_preset = (cal_orient_preset + 7) % 8;
            sensors.setPreviewMagOrientation(cal_orient_preset % 4, cal_orient_preset >= 4);
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            cal_orient_preset = (cal_orient_preset + 1) % 8;
            sensors.setPreviewMagOrientation(cal_orient_preset % 4, cal_orient_preset >= 4);
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            sensors.saveOrientationMode(cal_orient_preset % 4, cal_orient_preset >= 4);
            soundManager.playNavSelect();
            showToast("ORIENT SAVED");
            compass_state = CompassState::PAGE_CAL_MENU;
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
            } else if (imu_subapp_selection == 1) {
                imu_subapp = Imu6500SubApp::SUBAPP_AIRMOUSE;
                if (!airMouse.isEnabled()) airMouse.start();
            } else {
                imu_subapp = Imu6500SubApp::SUBAPP_MOUSE_SETTINGS;
                mouse_settings_selection = 0;
            }
            needs_redraw = true;
        }
        return;
    }

    if (imu_subapp == Imu6500SubApp::SUBAPP_AIRMOUSE) {
        handleAirMouseInput();
        return;
    }

    if (imu_subapp == Imu6500SubApp::SUBAPP_MOUSE_SETTINGS) {
        handleMouseSettingsInput();
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
            if (compass_menu_selection >= compass_menu_offset + 4) compass_menu_offset = compass_menu_selection - 3;
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (compass_menu_selection == 0) {
                if (sensors.isBmeOk()) sensors.zeroAltitude();
                else showToast("[BME UNLINKED]", 1500);
            } else if (compass_menu_selection == 1) sensors.zeroLevel();
            else if (compass_menu_selection == 2) {
                sensors.calibrateGyro();
                showToast("[GYRO CAL OK]", 1500);
            }
            else if (compass_menu_selection == 3) sensors.calibrateAccel();
            else if (compass_menu_selection == 4) {
                static const bool kImuFlags[8][4] = {
                    {false, false, false, false},
                    {true,  false, true,  false},
                    {false, true,  true,  false},
                    {true,  true,  false, false},
                    {false, false, true,  true},
                    {true,  false, false, true},
                    {false, true,  false, true},
                    {true,  true,  true,  true}
                };
                imu_orient_preset = 0;
                for (int i = 0; i < 8; i++) {
                    if (kImuFlags[i][0] == sensors.getImuSwapXY() &&
                        kImuFlags[i][1] == sensors.getImuInvX() &&
                        kImuFlags[i][2] == sensors.getImuInvY() &&
                        kImuFlags[i][3] == sensors.getImuInvZ()) {
                        imu_orient_preset = i;
                        break;
                    }
                }
                sensors.setPreviewImuOrientation(kImuFlags[imu_orient_preset][0],
                                                 kImuFlags[imu_orient_preset][1],
                                                 kImuFlags[imu_orient_preset][2],
                                                 kImuFlags[imu_orient_preset][3]);
                motion_state = MotionState::PAGE_ORIENTATION_3D;
            }
            else if (compass_menu_selection == 5) sensors.setImuSwapXY(!sensors.getImuSwapXY());
            else if (compass_menu_selection == 6) sensors.setImuInvX(!sensors.getImuInvX());
            else if (compass_menu_selection == 7) sensors.setImuInvZ(!sensors.getImuInvZ());
            needs_redraw = true;
        }
    } else if (motion_state == MotionState::PAGE_ORIENTATION_3D) {
        static const bool kImuFlags[8][4] = {
            {false, false, false, false},
            {true,  false, true,  false},
            {false, true,  true,  false},
            {true,  true,  false, false},
            {false, false, true,  true},
            {true,  false, false, true},
            {false, true,  false, true},
            {true,  true,  true,  true}
        };
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            imu_orient_preset = (imu_orient_preset + 7) % 8;
            sensors.setPreviewImuOrientation(kImuFlags[imu_orient_preset][0],
                                             kImuFlags[imu_orient_preset][1],
                                             kImuFlags[imu_orient_preset][2],
                                             kImuFlags[imu_orient_preset][3]);
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            imu_orient_preset = (imu_orient_preset + 1) % 8;
            sensors.setPreviewImuOrientation(kImuFlags[imu_orient_preset][0],
                                             kImuFlags[imu_orient_preset][1],
                                             kImuFlags[imu_orient_preset][2],
                                             kImuFlags[imu_orient_preset][3]);
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            sensors.saveImuOrientation(kImuFlags[imu_orient_preset][0],
                                       kImuFlags[imu_orient_preset][1],
                                       kImuFlags[imu_orient_preset][2],
                                       kImuFlags[imu_orient_preset][3]);
            soundManager.playNavSelect();
            showToast("IMU ORIENT SAVED");
            motion_state = MotionState::PAGE_SETTINGS;
            needs_redraw = true;
        }
    }
}

void UICore::handleAirMouseInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    ButtonEvent cancel_evt = btnManager.getEvent(BTN_ID_CANCEL);

    // Control button (CANCEL) long press: Exit Air Mouse and tear down BLE
    if (cancel_evt == BTN_EVT_LONG_PRESS || cancel_evt == BTN_EVT_DOUBLE_TAP) {
        soundManager.playNavBack();
        airMouse.stop();
        imu_subapp = Imu6500SubApp::SUBAPP_MENU;
        showToast("[AIR MOUSE EXIT]", 1000);
        needs_redraw = true;
        return;
    }

    // OK button long press: Toggle Pause / Resume
    if (ok_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavSelect();
        airMouse.toggleMovementPause();
        showToast(airMouse.isMovementActive() ? "[POINTER RESUMED]" : "[POINTER PAUSED]", 1000);
        needs_redraw = true;
        return;
    }

    // When ONLY paused: allow navigating and changing the Air Mouse settings!
    if (airMouse.isMovementPaused()) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            mouse_settings_selection--;
            if (mouse_settings_selection < 0) mouse_settings_selection = 0;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            mouse_settings_selection++;
            if (mouse_settings_selection > 8) mouse_settings_selection = 8;
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            if (mouse_settings_selection == 0) {
                airMouse.cycleSensitivitySlider(true);
            } else if (mouse_settings_selection == 1) {
                airMouse.cycleDeadZone(true);
            } else if (mouse_settings_selection == 2) {
                airMouse.cycleAntiDeadZone(true);
            } else if (mouse_settings_selection == 3) {
                airMouse.togglePrecisionMode();
                showToast(airMouse.getPrecisionMode() ? "[PRECISION ON]" : "[PRECISION OFF]", 1000);
            } else if (mouse_settings_selection == 4) {
                airMouse.toggleCombinedYawRoll();
                showToast(airMouse.getCombinedYawRoll() ? "[YAW+ROLL ON]" : "[YAW+ROLL OFF]", 1000);
            } else if (mouse_settings_selection == 5) {
                airMouse.toggleSwapXY();
                showToast(airMouse.getSwapXY() ? "[SWAP X/Y ON]" : "[SWAP X/Y OFF]", 1000);
            } else if (mouse_settings_selection == 6) {
                airMouse.toggleInvX();
                showToast(airMouse.getInvX() ? "[INVERT X ON]" : "[INVERT X OFF]", 1000);
            } else if (mouse_settings_selection == 7) {
                airMouse.toggleInvY();
                showToast(airMouse.getInvY() ? "[INVERT Y ON]" : "[INVERT Y OFF]", 1000);
            } else if (mouse_settings_selection == 8) {
                airMouse.recenter();
                showToast("[GYRO RECENTERED]", 1000);
            }
            needs_redraw = true;
        } else if (cancel_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavMove();
            if (mouse_settings_selection == 0) {
                airMouse.cycleSensitivitySlider(false);
            } else if (mouse_settings_selection == 1) {
                airMouse.cycleDeadZone(false);
            } else if (mouse_settings_selection == 2) {
                airMouse.cycleAntiDeadZone(false);
            } else if (mouse_settings_selection == 3) {
                airMouse.togglePrecisionMode();
                showToast(airMouse.getPrecisionMode() ? "[PRECISION ON]" : "[PRECISION OFF]", 1000);
            } else if (mouse_settings_selection == 4) {
                airMouse.toggleCombinedYawRoll();
                showToast(airMouse.getCombinedYawRoll() ? "[YAW+ROLL ON]" : "[YAW+ROLL OFF]", 1000);
            } else if (mouse_settings_selection == 5) {
                airMouse.toggleSwapXY();
                showToast(airMouse.getSwapXY() ? "[SWAP X/Y ON]" : "[SWAP X/Y OFF]", 1000);
            } else if (mouse_settings_selection == 6) {
                airMouse.toggleInvX();
                showToast(airMouse.getInvX() ? "[INVERT X ON]" : "[INVERT X OFF]", 1000);
            } else if (mouse_settings_selection == 7) {
                airMouse.toggleInvY();
                showToast(airMouse.getInvY() ? "[INVERT Y ON]" : "[INVERT Y OFF]", 1000);
            } else if (mouse_settings_selection == 8) {
                airMouse.recenter();
                showToast("[GYRO RECENTERED]", 1000);
            }
            needs_redraw = true;
        }
        return;
    }

    // Active (NOT paused):
    // Single click CANCEL toggles between Pointer Mode and Scroll Mode
    if (cancel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        airMouse.toggleMode();
        showToast(airMouse.getMode() == AirMouseMode::SCROLL ? "[SCROLL MODE]" : "[POINTER MODE]", 1000);
        needs_redraw = true;
        return;
    }

    // Mode-specific button controls:
    if (airMouse.getMode() == AirMouseMode::POINTER) {
        // Continuous Button Hold (Full mouse drag & drop, text select capabilities):
        // UP: Left click & hold (Button 1: 0x01)
        // DN: Right click & hold (Button 2: 0x02)
        // Short OK: Back click (Button 4: 0x08)
        bool up_held = btnManager.isPressed(BTN_ID_UP);
        bool dn_held = btnManager.isPressed(BTN_ID_DN);

        airMouse.setButton(MOUSE_BUTTON_LEFT, up_held);
        airMouse.setButton(MOUSE_BUTTON_RIGHT, dn_held);

        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            airMouse.clickBack();
            needs_redraw = true;
        }

        if (up_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            needs_redraw = true;
        }
    } else {
        // SCROLL Mode: UP scrolls up, DOWN scrolls down, Short OK clicks Back
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            soundManager.playNavMove();
            airMouse.scrollUp();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            soundManager.playNavMove();
            airMouse.scrollDown();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            airMouse.clickBack();
            needs_redraw = true;
        }
    }
}

void UICore::handleMouseSettingsInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    ButtonEvent cancel_evt = btnManager.getEvent(BTN_ID_CANCEL);

    if (cancel_evt == BTN_EVT_SHORT_PRESS || cancel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        imu_subapp = Imu6500SubApp::SUBAPP_MENU;
        needs_redraw = true;
        return;
    }

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        mouse_settings_selection--;
        if (mouse_settings_selection < 0) mouse_settings_selection = 0;
        soundManager.playNavMove();
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        mouse_settings_selection++;
        if (mouse_settings_selection > 9) mouse_settings_selection = 9;
        soundManager.playNavMove();
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (mouse_settings_selection == 0) {
            airMouse.cycleSensitivitySlider(true);
        } else if (mouse_settings_selection == 1) {
            airMouse.cycleDeadZone(true);
        } else if (mouse_settings_selection == 2) {
            airMouse.cycleAntiDeadZone(true);
        } else if (mouse_settings_selection == 3) {
            airMouse.togglePrecisionMode();
            showToast(airMouse.getPrecisionMode() ? "[PRECISION ON]" : "[PRECISION OFF]", 1000);
        } else if (mouse_settings_selection == 4) {
            airMouse.toggleCombinedYawRoll();
            showToast(airMouse.getCombinedYawRoll() ? "[YAW+ROLL ON]" : "[YAW+ROLL OFF]", 1000);
        } else if (mouse_settings_selection == 5) {
            airMouse.toggleSwapXY();
            showToast(airMouse.getSwapXY() ? "[SWAP X/Y ON]" : "[SWAP X/Y OFF]", 1000);
        } else if (mouse_settings_selection == 6) {
            airMouse.toggleInvX();
            showToast(airMouse.getInvX() ? "[INVERT X ON]" : "[INVERT X OFF]", 1000);
        } else if (mouse_settings_selection == 7) {
            airMouse.toggleInvY();
            showToast(airMouse.getInvY() ? "[INVERT Y ON]" : "[INVERT Y OFF]", 1000);
        } else if (mouse_settings_selection == 8) {
            airMouse.recenter();
            showToast("[GYRO RECENTERED]", 1000);
        } else if (mouse_settings_selection == 9) {
            imu_subapp = Imu6500SubApp::SUBAPP_AIRMOUSE;
            if (!airMouse.isEnabled()) airMouse.start();
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

void UICore::handleWirelessInput() {
    switch (recon_submenu) {
        case ReconSubmenu::MAIN: handleReconMainInput(); break;
        case ReconSubmenu::BLE_LIST: handleBleListInput(); break;
        case ReconSubmenu::BLE_RADAR: handleBleRadarInput(); break;
        case ReconSubmenu::WIFI_SPECTRUM: handleWifiSpectrumInput(); break;
        case ReconSubmenu::DEAUTH_DETECT: handleDeauthDetectInput(); break;
        case ReconSubmenu::PKT_MONITOR: handlePacketMonitorInput(); break;
    }
}

void UICore::handleReconMainInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (recon_selection > 0) {
            recon_selection--;
            if (recon_selection < recon_scroll_offset) recon_scroll_offset = recon_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (recon_selection < RECON_MAIN_ITEM_COUNT - 1) {
            recon_selection++;
            if (recon_selection >= recon_scroll_offset + 4) recon_scroll_offset = recon_selection - 3;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        switch (recon_selection) {
            case 0:
                recon_submenu = ReconSubmenu::BLE_LIST;
                ble_list_selection = 0;
                ble_list_scroll_offset = 0;
                wirelessRecon.startBleScan();
                break;
            case 1:
                recon_submenu = ReconSubmenu::WIFI_SPECTRUM;
                wirelessRecon.startChannelScan();
                break;
            case 2:
                recon_submenu = ReconSubmenu::DEAUTH_DETECT;
                wirelessRecon.startDeauthMonitor(1);
                break;
            case 3:
                recon_submenu = ReconSubmenu::PKT_MONITOR;
                wirelessRecon.startPacketMonitor(1);
                break;
        }
        needs_redraw = true;
    }
}

void UICore::handleBleListInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    int count = wirelessRecon.getBleTargetCount();

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (ble_list_selection > 0) {
            ble_list_selection--;
            if (ble_list_selection < ble_list_scroll_offset) ble_list_scroll_offset = ble_list_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (ble_list_selection < count - 1) {
            ble_list_selection++;
            if (ble_list_selection >= ble_list_scroll_offset + 4) ble_list_scroll_offset = ble_list_selection - 3;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (count > 0) {
            wirelessRecon.lockTarget(ble_list_selection);
            recon_submenu = ReconSubmenu::BLE_RADAR;
            showToast("[TARGET LOCKED]", 1000);
        } else {
            wirelessRecon.startBleScan();
            showToast("[SCANNING BLE]", 1000);
        }
        needs_redraw = true;
    }
}

void UICore::handleBleRadarInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    int count = wirelessRecon.getBleTargetCount();

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (count > 1) {
            int next_idx = (wirelessRecon.getLockedTargetIdx() + 1) % count;
            wirelessRecon.lockTarget(next_idx);
            ble_list_selection = next_idx;
            showToast("[TARGET CYCLED]", 800);
        } else {
            showToast("[TARGET LOCKED]", 800);
        }
        needs_redraw = true;
    } else if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (count > 1) {
            int prev_idx = (wirelessRecon.getLockedTargetIdx() + count - 1) % count;
            wirelessRecon.lockTarget(prev_idx);
            ble_list_selection = prev_idx;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (count > 1) {
            int next_idx = (wirelessRecon.getLockedTargetIdx() + 1) % count;
            wirelessRecon.lockTarget(next_idx);
            ble_list_selection = next_idx;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    }
}

void UICore::handleWifiSpectrumInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        wirelessRecon.startChannelScan();
        showToast("[SCANNING...]", 1000);
        needs_redraw = true;
    }
}

void UICore::handleDeauthDetectInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        wirelessRecon.resetDeauthStats();
        showToast("[ALERTS RESET]", 1000);
        needs_redraw = true;
    } else if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        wirelessRecon.cycleChannel();
        soundManager.playNavMove();
        char buf[20];
        snprintf(buf, sizeof(buf), "[CH %u MON]", wirelessRecon.getActiveChannel());
        showToast(buf, 800);
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        wirelessRecon.cycleChannel();
        soundManager.playNavMove();
        char buf[20];
        snprintf(buf, sizeof(buf), "[CH %u MON]", wirelessRecon.getActiveChannel());
        showToast(buf, 800);
        needs_redraw = true;
    }
}

void UICore::handlePacketMonitorInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        wirelessRecon.cycleChannel();
        soundManager.playNavSelect();
        char buf[20];
        snprintf(buf, sizeof(buf), "[CH %u LOCKED]", wirelessRecon.getActiveChannel());
        showToast(buf, 800);
        needs_redraw = true;
    } else if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        wirelessRecon.cycleChannel();
        soundManager.playNavMove();
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        wirelessRecon.cycleChannel();
        soundManager.playNavMove();
        needs_redraw = true;
    }
}

void UICore::handleMochiInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);
    ButtonEvent cancel_evt = btnManager.getEvent(BTN_ID_CANCEL);

    if (mochiPet.isPlayingAnim()) {
        if (cancel_evt == BTN_EVT_LONG_PRESS) {
            mochiPet.stopAnim();
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 14;
            menu_scroll_offset = 11;
            needs_redraw = true;
            return;
        }
        if (up_evt != BTN_EVT_NONE || dn_evt != BTN_EVT_NONE || ok_evt != BTN_EVT_NONE || cancel_evt != BTN_EVT_NONE) {
            mochiPet.stopAnim();
            needs_redraw = true;
            return;
        }
    }

    MochiSubmode sub = mochiPet.getSubmode();

    if (sub == MochiSubmode::INTERACTIVE) {
        if (up_evt == BTN_EVT_SHORT_PRESS) {
            mochiPet.nextEmote();
            needs_redraw = true;
        } else if (up_evt == BTN_EVT_LONG_PRESS) {
            mochiPet.setSubmode(MochiSubmode::EMOTE_PICKER);
            soundManager.playNavSelect();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
            mochiPet.prevEmote();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_LONG_PRESS) {
            mochiPet.setSubmode(MochiSubmode::HELMET_PICKER);
            soundManager.playNavSelect();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            mochiPet.pet();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_LONG_PRESS) {
            mochiPet.feed();
            needs_redraw = true;
        } else if (cancel_evt == BTN_EVT_SHORT_PRESS) {
            mochiPet.toggleHud();
            soundManager.playNavSelect();
            needs_redraw = true;
        } else if (cancel_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 14;
            menu_scroll_offset = 11;
            needs_redraw = true;
        }
    } else if (sub == MochiSubmode::STATS_HUD) {
        if (cancel_evt == BTN_EVT_SHORT_PRESS || ok_evt == BTN_EVT_SHORT_PRESS) {
            mochiPet.toggleHud();
            soundManager.playNavSelect();
            needs_redraw = true;
        } else if (up_evt == BTN_EVT_SHORT_PRESS) {
            mochiPet.nextHelmet();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
            mochiPet.prevHelmet();
            needs_redraw = true;
        } else if (cancel_evt == BTN_EVT_LONG_PRESS) {
            soundManager.playNavBack();
            current_state = UIState::MAIN_MENU;
            menu_selection = 14;
            menu_scroll_offset = 11;
            needs_redraw = true;
        }
    } else if (sub == MochiSubmode::EMOTE_PICKER || sub == MochiSubmode::HELMET_PICKER) {
        if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
            mochiPet.pickerUp();
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
            mochiPet.pickerDown();
            soundManager.playNavMove();
            needs_redraw = true;
        } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
            mochiPet.pickerSelect();
            soundManager.playNavSelect();
            needs_redraw = true;
        } else if (cancel_evt == BTN_EVT_SHORT_PRESS || cancel_evt == BTN_EVT_LONG_PRESS) {
            mochiPet.setSubmode(MochiSubmode::INTERACTIVE);
            soundManager.playNavBack();
            needs_redraw = true;
        }
    }
}

void UICore::handleVibrationInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (vibe_selection > 0) {
            vibe_selection--;
            if (vibe_selection < vibe_offset) vibe_offset = vibe_selection;
        } else {
            vibe_selection = VIBE_MENU_ITEM_COUNT - 1;
            vibe_offset = (VIBE_MENU_ITEM_COUNT > 4) ? (VIBE_MENU_ITEM_COUNT - 4) : 0;
        }
        soundManager.playNavMove();
        vibrationManager.triggerHapticClick();
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (vibe_selection < VIBE_MENU_ITEM_COUNT - 1) {
            vibe_selection++;
            if (vibe_selection >= vibe_offset + 4) vibe_offset = vibe_selection - 3;
        } else {
            vibe_selection = 0;
            vibe_offset = 0;
        }
        soundManager.playNavMove();
        vibrationManager.triggerHapticClick();
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        switch (vibe_selection) {
            case 0: // CLICK / TICK
                vibrationManager.triggerPattern(VibePattern::CLICK);
                showToast("CLICK 35ms", 800);
                break;
            case 1: // DOUBLE PULSE
                vibrationManager.triggerPattern(VibePattern::DOUBLE_PULSE);
                showToast("DOUBLE PULSE", 800);
                break;
            case 2: // TACTICAL ALERT
                vibrationManager.triggerPattern(VibePattern::ALERT);
                showToast("ALERT BUZZ", 800);
                break;
            case 3: // HEARTBEAT
                vibrationManager.triggerPattern(VibePattern::HEARTBEAT);
                showToast("HEARTBEAT", 800);
                break;
            case 4: // SOS MORSE
                vibrationManager.triggerPattern(VibePattern::SOS_MORSE);
                showToast("... --- ...", 1200);
                break;
            case 5: // RAMP INTENSITY
                vibrationManager.triggerPattern(VibePattern::RAMP_UP);
                showToast("PWM RAMP", 800);
                break;
            case 6: // CONTINUOUS RUN
                if (vibrationManager.isVibrating() && vibrationManager.getCurrentPattern() == VibePattern::CONTINUOUS) {
                    vibrationManager.stop();
                    showToast("MOTOR STOPPED", 800);
                } else {
                    vibrationManager.triggerPattern(VibePattern::CONTINUOUS);
                    showToast("CONTINUOUS (OK: stop)", 1200);
                }
                break;
            case 7: // STRENGTH
            {
                uint8_t cur = vibrationManager.getIntensity();
                uint8_t next = (cur == 25) ? 50 : (cur == 50) ? 75 : (cur == 75) ? 100 : 25;
                vibrationManager.setIntensity(next);
                vibrationManager.triggerPulse(80, 100);
                break;
            }
            case 8: // BUTTON HAPTICS
            {
                bool next = !vibrationManager.isButtonHapticsEnabled();
                vibrationManager.setButtonHaptics(next);
                showToast(next ? "BTN VIBE: ON" : "BTN VIBE: OFF", 800);
                if (next) vibrationManager.triggerHapticClick();
                break;
            }
        }
        needs_redraw = true;
    }
}

