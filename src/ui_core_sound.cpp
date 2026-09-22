#include "ui_core.h"
#include "sound_manager.h"
#include "settings_data.h"
#include "button_manager.h"

static const uint16_t NOTE_FREQS[] = { 262, 294, 330, 349, 392, 440, 494, 2700 }; // C, D, E, F, G, A, B, 2700Hz
static const char* const NOTE_NAMES[] = { "C", "D", "E", "F", "G", "A", "B", "RES" };
static const uint16_t DUR_MS[] = { 100, 200, 400, 800 };
static const char* const DUR_NAMES[] = { "1/8", "1/4", "1/2", "1/1" };

void UICore::handleSoundInput() {
    if (sound_submenu == SoundSubmenu::METRONOME && metronome_active) {
        uint32_t interval = 60000 / metronome_bpm;
        if (millis() - last_metronome_tick >= interval) {
            last_metronome_tick = millis();
            metronome_beat = (metronome_beat % 4) + 1;
            uint16_t pitch = (metronome_beat == 1) ? 3200 : 2200;
            soundManager.playTone(pitch, 25);
            needs_redraw = true;
        }
    }

    if (sound_submenu == SoundSubmenu::LAB && lab_sweep_active) {
        if (millis() - last_sweep_time >= 50) {
            last_sweep_time = millis();
            lab_sweep_freq += 25;
            if (lab_sweep_freq > 3500) {
                lab_sweep_freq = 2000;
                lab_sweep_active = false;
                soundManager.stop();
                showToast("[SWEEP COMPLETE]", 1200);
            } else {
                soundManager.playTone(lab_sweep_freq, 40);
            }
            needs_redraw = true;
        }
    }

    switch (sound_submenu) {
        case SoundSubmenu::MAIN: handleSoundMainInput(); break;
        case SoundSubmenu::SETTINGS: handleSoundSettingsInput(); break;
        case SoundSubmenu::EFFECTS: handleSoundEffectsInput(); break;
        case SoundSubmenu::CREATOR_LIST: handleSoundCreatorListInput(); break;
        case SoundSubmenu::COMPOSER: handleSoundComposerInput(); break;
        case SoundSubmenu::LAB: handleSoundLabInput(); break;
        case SoundSubmenu::METRONOME: handleMetronomeInput(); break;
        case SoundSubmenu::MORSE: handleMorseInput(); break;
        default: break;
    }
}

void UICore::handleSoundMainInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (sound_selection > 0) {
            sound_selection--;
            if (sound_selection < sound_scroll_offset) sound_scroll_offset = sound_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (sound_selection < SOUND_MAIN_ITEM_COUNT - 1) {
            sound_selection++;
            if (sound_selection >= sound_scroll_offset + 3) sound_scroll_offset = sound_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        switch (sound_selection) {
            case 0: sound_submenu = SoundSubmenu::SETTINGS; sound_selection = 0; sound_scroll_offset = 0; break;
            case 1: sound_submenu = SoundSubmenu::EFFECTS; sound_selection = 0; sound_scroll_offset = 0; break;
            case 2:
                fileManager.create("/sounds");
                sound_submenu = SoundSubmenu::CREATOR_LIST;
                sound_selection = 0;
                sound_scroll_offset = 0;
                break;
            case 3: sound_submenu = SoundSubmenu::COMPOSER; sound_selection = 0; sound_scroll_offset = 0; break;
            case 4: sound_submenu = SoundSubmenu::LAB; sound_selection = 0; sound_scroll_offset = 0; break;
            case 5: sound_submenu = SoundSubmenu::METRONOME; sound_selection = 0; sound_scroll_offset = 0; break;
            case 6: sound_submenu = SoundSubmenu::MORSE; sound_selection = 0; sound_scroll_offset = 0; break;
        }
        needs_redraw = true;
    }
}

void UICore::handleSoundSettingsInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (sound_selection > 0) {
            sound_selection--;
            if (sound_selection < sound_scroll_offset) sound_scroll_offset = sound_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (sound_selection < SOUND_SETTINGS_ITEM_COUNT - 1) {
            sound_selection++;
            if (sound_selection >= sound_scroll_offset + 3) sound_scroll_offset = sound_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (sound_selection == 0) {
            soundManager.setMasterSwitch(!soundManager.isMasterSwitchOn());
        } else if (sound_selection == 1) {
            int new_vol = soundManager.getVolumePct() + 25;
            if (new_vol > 100) new_vol = 0;
            soundManager.setVolumePct(new_vol);
        } else if (sound_selection == 2) {
            soundManager.setButtonSoundsEnabled(!soundManager.isButtonSoundsEnabled());
        } else if (sound_selection == 3) {
            soundManager.setNotificationsEnabled(!soundManager.isNotificationsEnabled());
        } else if (sound_selection == 4) {
            int st = (int)soundManager.getStyle() + 1;
            if (st > 3) st = 0;
            soundManager.setStyle((SoundStyle)st);
        }
        needs_redraw = true;
    }
}

void UICore::handleSoundEffectsInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (sound_selection > 0) {
            sound_selection--;
            if (sound_selection < sound_scroll_offset) sound_scroll_offset = sound_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (sound_selection < SOUND_EFFECTS_ITEM_COUNT - 1) {
            sound_selection++;
            if (sound_selection >= sound_scroll_offset + 3) sound_scroll_offset = sound_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        switch (sound_selection) {
            case 0: soundManager.playBoot(); break;
            case 1: soundManager.playWake(); break;
            case 2: soundManager.playSleep(); break;
            case 3: soundManager.playNotification(); break;
            case 4: soundManager.playWarning(); break;
            case 5: soundManager.playAlert(); break;
            case 6: soundManager.playQBranch(); break;
        }
        needs_redraw = true;
    }
}

static void onMelodyNameEntered(bool success, const String& name) {
    if (success && name.length() > 0) {
        String path = "/sounds/" + name + ".mel";
        fileManager.write(path, "2700,100\n0,50\n2400,100\n0,50\n2700,200\n");
        ui.showToast("[MELODY SAVED]", 1200);
        ui.setSoundSubmenu(SoundSubmenu::CREATOR_LIST);
    }
}

void UICore::handleSoundCreatorListInput() {
    FileInfo entries[16];
    size_t count = fileManager.listDir("/sounds", entries, 16);

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    int total_items = count + 1; // +1 for "New Melody"

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (sound_selection > 0) {
            sound_selection--;
            if (sound_selection < sound_scroll_offset) sound_scroll_offset = sound_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (sound_selection < total_items - 1) {
            sound_selection++;
            if (sound_selection >= sound_scroll_offset + 3) sound_scroll_offset = sound_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (sound_selection == 0) {
            openKeyboard("my_theme", "Melody Name", KeyboardMode::ALPHA, false, 20, onMelodyNameEntered);
        } else {
            String path = "/sounds/" + entries[sound_selection - 1].name;
            soundManager.playMelodyFile(path);
            showToast(("[PLAY " + entries[sound_selection - 1].name + "]").c_str(), 1000);
        }
        needs_redraw = true;
    }
}

void UICore::handleSoundComposerInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS) {
        composer_note_idx = (composer_note_idx + 1) % 8;
        soundManager.playTone(NOTE_FREQS[composer_note_idx] * composer_octave, 80);
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
        composer_dur_idx = (composer_dur_idx + 1) % 4;
        soundManager.playNavMove();
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playTone(NOTE_FREQS[composer_note_idx] * composer_octave, DUR_MS[composer_dur_idx]);
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_LONG_PRESS) {
        composer_octave = (composer_octave == 1) ? 2 : 1;
        showToast(composer_octave == 1 ? "[OCTAVE 1]" : "[OCTAVE 2]", 1000);
        needs_redraw = true;
    }
}

void UICore::handleSoundLabInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (sound_selection > 0) {
            sound_selection--;
            if (sound_selection < sound_scroll_offset) sound_scroll_offset = sound_selection;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (sound_selection < SOUND_LAB_ITEM_COUNT - 1) {
            sound_selection++;
            if (sound_selection >= sound_scroll_offset + 3) sound_scroll_offset = sound_selection - 2;
            soundManager.playNavMove();
            needs_redraw = true;
        }
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        if (sound_selection == 0) {
            soundManager.playTone(2700, 300);
            showToast("[2700 Hz RESONANCE]", 1000);
        } else if (sound_selection == 1) {
            lab_sweep_active = true;
            lab_sweep_freq = 2000;
            last_sweep_time = millis();
            showToast("[SWEEP 2000-3500 Hz]", 1000);
        } else if (sound_selection == 2) {
            lab_duty = (lab_duty == 50) ? 75 : ((lab_duty == 75) ? 25 : 50);
            showToast(("[DUTY " + String(lab_duty) + "%]").c_str(), 1000);
        } else if (sound_selection == 3) {
            soundManager.playQBranch();
            showToast("[BUZZER DIAG PASS]", 1200);
        }
        needs_redraw = true;
    }
}

void UICore::handleMetronomeInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (metronome_bpm < 240) metronome_bpm += 5;
        soundManager.playNavMove();
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (metronome_bpm > 40) metronome_bpm -= 5;
        soundManager.playNavMove();
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        metronome_active = !metronome_active;
        metronome_beat = 0;
        last_metronome_tick = millis();
        soundManager.playNavSelect();
        needs_redraw = true;
    }
}

void UICore::handleMorseInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playMorse("CQ CQ Q-WATCH");
        showToast("[TX CQ CQ]", 1200);
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playMorse("SOS");
        showToast("[TX SOS]", 1200);
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playMorse("007");
        showToast("[TX 007]", 1200);
        needs_redraw = true;
    }
}
