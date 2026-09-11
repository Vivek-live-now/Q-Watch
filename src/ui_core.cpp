#include "ui_core.h"
#include "sensors.h"
#include "button_manager.h"
#include "hw_config.h"
#include "led_manager.h"
#include "sound_manager.h"
#include "driver/rtc_io.h"

UICore ui;

UICore::UICore() :
    current_state(UIState::APP_HOME),
    menu_selection(0),
    menu_scroll_offset(0),
    edit_value(5), compass_state(CompassState::PAGE_MAIN), compass_menu_selection(0), compass_menu_offset(0), motion_state(MotionState::PAGE_LEVEL),
    needs_redraw(true) {}

void UICore::begin() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("Woke up from deep sleep via SELECT button (GPIO21)!");
    }
}

void UICore::loop() {
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

    // If the cursor moves above the current viewing window, scroll up
    if (menu_selection < menu_scroll_offset) {
        menu_scroll_offset = menu_selection;
    }
    soundManager.playNavMove();
    needs_redraw = true;
}

void UICore::processNavDown() {
    int max_items = (current_state == UIState::MAIN_MENU) ? MAIN_MENU_ITEM_COUNT : SETTINGS_MENU_ITEM_COUNT;

    menu_selection++;
    if (menu_selection >= max_items) {
        menu_selection = max_items - 1;
    }

    // Display shows 3 items at a time (indices 0, 1, 2 relative to offset)
    // If cursor reaches index 3 relative to offset, we must scroll down by 1.
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

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
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
            case 10: current_state = UIState::APP_SETTINGS; menu_selection=0; menu_scroll_offset=0; break;
            case 11: current_state = UIState::APP_ABOUT; break;
        }
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        current_state = UIState::APP_HOME;
        needs_redraw = true;
    }
}

void UICore::handleGenericAppInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_LONG_PRESS) {
        current_state = UIState::APP_HOME;
        needs_redraw = true;
    }
}

void UICore::handleSettingsMenuInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) processNavUp();

    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) processNavDown();

    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (menu_selection == 3) { // "Sleep"
            enterDeepSleep();
        } else {
            current_state = UIState::VALUE_EDIT;
            needs_redraw = true;
        }
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        current_state = UIState::MAIN_MENU;
        menu_selection = 11; // Reset cursor to Settings in main menu
        menu_scroll_offset = 7; // Scroll so Settings is at the bottom of the 3-item list (index 8, offset 6)
        // Wait, 8 - 2 = 6. Let's fix this mathematically so it's always correct:
        menu_scroll_offset = menu_selection - 2;
        if(menu_scroll_offset < 0) menu_scroll_offset = 0;
        needs_redraw = true;
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
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        current_state = UIState::APP_SETTINGS;
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
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
        needs_redraw = true; // Constantly redraw progress bar
    }
    else if (compass_state == CompassState::CAL_RESULT) {
        if (up_evt == BTN_EVT_SHORT_PRESS) {
            // YES
            sensors.saveCurrentCalibration();
            compass_state = CompassState::PAGE_CAL_MENU;
            needs_redraw = true;
        } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
            // NO
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
        needs_redraw = true; // Constantly redraw telemetry
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
            compass_menu_selection = 0; // reuse this variable for memory saving
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
            s = (s + 1) % 4; // 0: SILENT, 1: MODERN, 2: TACTICAL, 3: RETRO
            soundManager.setStyle((SoundStyle)s);
        }
        needs_redraw = true;
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        current_state = UIState::MAIN_MENU;
        needs_redraw = true;
    }
}
