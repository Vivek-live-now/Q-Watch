#include "ui_core.h"
#include "sound_manager.h"
#include "settings_data.h"
#include "button_manager.h"

static const uint16_t NOTE_BASE_FREQS[] = { 262, 294, 330, 349, 392, 440, 494, 0 }; // C4, D4, E4, F4, G4, A4, B4, REST (0Hz)
static const char* const NOTE_NAMES[] = { "C", "D", "E", "F", "G", "A", "B", "REST" };
static const uint16_t DUR_MS[] = { 100, 200, 400, 800 };
static const char* const DUR_NAMES[] = { "1/8", "1/4", "1/2", "1/1" };

static const uint16_t CREATOR_PITCH_LIST[] = { 0, 262, 294, 330, 349, 392, 440, 494, 523, 587, 659, 698, 784, 880, 988, 2700 };
static const char* const CREATOR_PITCH_NAMES[] = { "REST", "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5", "D5", "E5", "F5", "G5", "A5", "B5", "2700Hz" };
static const int CREATOR_PITCH_COUNT = 16;

static uint16_t getComposerFreq(int note_idx, int octave) {
    if (note_idx == 7) return 0; // REST
    return NOTE_BASE_FREQS[note_idx] * octave;
}

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
        case SoundSubmenu::CREATOR_EDIT: handleSoundCreatorEditInput(); break;
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
        ui.setActiveMelodyName(name);
        String path = "/sounds/" + name + ".mel";
        if (!fileManager.exists(path)) {
            fileManager.write(path, "2700,200\n0,50\n2400,200\n");
        }
        ui.setSoundSubmenu(SoundSubmenu::CREATOR_EDIT);
        ui.showToast("[EDITING MELODY]", 1200);
    }
}

void UICore::handleSoundCreatorListInput() {
    FileInfo entries[16];
    size_t count = fileManager.listDir("/sounds", entries, 16);

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    int total_items = count + 1; // +1 for "+ New Melody"

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
            String full_filename = entries[sound_selection - 1].name;
            String mel_name = full_filename;
            if (mel_name.endsWith(".mel")) mel_name = mel_name.substring(0, mel_name.length() - 4);
            active_melody_name = mel_name;

            // Load melody notes into creator buffer
            String path = "/sounds/" + full_filename;
            String content = fileManager.read(path);
            creator_count = 0;
            int pos = 0;
            while (pos < content.length() && creator_count < 64) {
                int next_nl = content.indexOf('\n', pos);
                if (next_nl == -1) next_nl = content.length();
                String line = content.substring(pos, next_nl);
                line.trim();
                pos = next_nl + 1;
                if (line.length() == 0 || line.startsWith("#")) continue;

                int comma = line.indexOf(',');
                if (comma != -1) {
                    uint16_t freq = line.substring(0, comma).toInt();
                    uint16_t dur = line.substring(comma + 1).toInt();
                    creator_notes[creator_count++] = { freq, dur };
                }
            }
            creator_cursor = 0;
            creator_edit_field = 0;
            sound_submenu = SoundSubmenu::CREATOR_EDIT;
            showToast(("[OPEN " + mel_name + "]").c_str(), 1000);
        }
        needs_redraw = true;
    }
}

void UICore::handleSoundCreatorEditInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (ok_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        creator_edit_field = (creator_edit_field + 1) % 6; // Fields: 0:NoteIdx, 1:Pitch, 2:Dur, 3:Insert, 4:Delete, 5:Save&Play
        needs_redraw = true;
        return;
    }

    if (ok_evt == BTN_EVT_LONG_PRESS) {
        // Save & Play Melody
        String path = "/sounds/" + active_melody_name + ".mel";
        String out = "";
        for (int i = 0; i < creator_count; i++) {
            out += String(creator_notes[i].freq) + "," + String(creator_notes[i].duration) + "\n";
        }
        fileManager.write(path, out);
        soundManager.playSequence(creator_notes, creator_count);
        showToast("[MELODY SAVED & PLAY]", 1200);
        needs_redraw = true;
        return;
    }

    if (up_evt == BTN_EVT_SHORT_PRESS || up_evt == BTN_EVT_REPEAT) {
        if (creator_edit_field == 0) { // Select Note Index
            if (creator_count > 0) {
                creator_cursor = (creator_cursor > 0) ? creator_cursor - 1 : creator_count - 1;
                if (creator_notes[creator_cursor].freq > 0) soundManager.playTone(creator_notes[creator_cursor].freq, 80);
            }
        } else if (creator_edit_field == 1) { // Pitch Selection
            if (creator_count > 0) {
                uint16_t cur_freq = creator_notes[creator_cursor].freq;
                int p_idx = 0;
                for (int i = 0; i < CREATOR_PITCH_COUNT; i++) {
                    if (CREATOR_PITCH_LIST[i] == cur_freq) { p_idx = i; break; }
                }
                p_idx = (p_idx + 1) % CREATOR_PITCH_COUNT;
                creator_notes[creator_cursor].freq = CREATOR_PITCH_LIST[p_idx];
                if (creator_notes[creator_cursor].freq > 0) soundManager.playTone(creator_notes[creator_cursor].freq, 100);
            }
        } else if (creator_edit_field == 2) { // Duration
            if (creator_count > 0) {
                uint16_t dur = creator_notes[creator_cursor].duration;
                if (dur <= 100) dur = 200;
                else if (dur <= 200) dur = 400;
                else if (dur <= 400) dur = 800;
                else dur = 100;
                creator_notes[creator_cursor].duration = dur;
            }
        } else if (creator_edit_field == 3) { // Insert Note
            if (creator_count < 64) {
                for (int i = creator_count; i > creator_cursor + 1; i--) {
                    creator_notes[i] = creator_notes[i - 1];
                }
                creator_notes[creator_cursor + 1] = { 2700, 200 };
                creator_count++;
                creator_cursor++;
                soundManager.playTone(2700, 100);
                showToast("[NOTE INSERTED]", 800);
            }
        } else if (creator_edit_field == 4) { // Delete Note
            if (creator_count > 1) {
                for (int i = creator_cursor; i < creator_count - 1; i++) {
                    creator_notes[i] = creator_notes[i + 1];
                }
                creator_count--;
                if (creator_cursor >= creator_count) creator_cursor = creator_count - 1;
                showToast("[NOTE DELETED]", 800);
            }
        } else if (creator_edit_field == 5) { // Play & Save
            soundManager.playSequence(creator_notes, creator_count);
        }
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS || dn_evt == BTN_EVT_REPEAT) {
        if (creator_edit_field == 0) { // Select Note Index
            if (creator_count > 0) {
                creator_cursor = (creator_cursor + 1) % creator_count;
                if (creator_notes[creator_cursor].freq > 0) soundManager.playTone(creator_notes[creator_cursor].freq, 80);
            }
        } else if (creator_edit_field == 1) { // Pitch Selection (Prev)
            if (creator_count > 0) {
                uint16_t cur_freq = creator_notes[creator_cursor].freq;
                int p_idx = 0;
                for (int i = 0; i < CREATOR_PITCH_COUNT; i++) {
                    if (CREATOR_PITCH_LIST[i] == cur_freq) { p_idx = i; break; }
                }
                p_idx = (p_idx > 0) ? p_idx - 1 : CREATOR_PITCH_COUNT - 1;
                creator_notes[creator_cursor].freq = CREATOR_PITCH_LIST[p_idx];
                if (creator_notes[creator_cursor].freq > 0) soundManager.playTone(creator_notes[creator_cursor].freq, 100);
            }
        } else if (creator_edit_field == 2) { // Duration
            if (creator_count > 0) {
                uint16_t dur = creator_notes[creator_cursor].duration;
                if (dur >= 800) dur = 400;
                else if (dur >= 400) dur = 200;
                else if (dur >= 200) dur = 100;
                else dur = 800;
                creator_notes[creator_cursor].duration = dur;
            }
        } else if (creator_edit_field == 3) { // Insert Note
            if (creator_count < 64) {
                for (int i = creator_count; i > creator_cursor + 1; i--) {
                    creator_notes[i] = creator_notes[i - 1];
                }
                creator_notes[creator_cursor + 1] = { 0, 100 }; // Insert Rest
                creator_count++;
                creator_cursor++;
                showToast("[REST INSERTED]", 800);
            }
        } else if (creator_edit_field == 4) { // Delete Note
            if (creator_count > 1) {
                for (int i = creator_cursor; i < creator_count - 1; i++) {
                    creator_notes[i] = creator_notes[i + 1];
                }
                creator_count--;
                if (creator_cursor >= creator_count) creator_cursor = creator_count - 1;
                showToast("[NOTE DELETED]", 800);
            }
        } else if (creator_edit_field == 5) { // Play & Save
            soundManager.playSequence(creator_notes, creator_count);
        }
        needs_redraw = true;
    }
}

static void onComposerSaveEntered(bool success, const String& name) {
    if (success && name.length() > 0) {
        String path = "/sounds/" + name + ".mel";
        String out = "";
        const SoundNote* notes = ui.getComposerNotes();
        for (int i = 0; i < ui.getComposerCount(); i++) {
            out += String(notes[i].freq) + "," + String(notes[i].duration) + "\n";
        }
        fileManager.write(path, out);
        ui.showToast("[COMPOSITION SAVED]", 1200);
    }
}

void UICore::handleSoundComposerInput() {
    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (up_evt == BTN_EVT_SHORT_PRESS) {
        composer_note_idx = (composer_note_idx + 1) % 8;
        uint16_t freq = getComposerFreq(composer_note_idx, composer_octave);
        if (freq > 0) soundManager.playTone(freq, 80);
        needs_redraw = true;
    } else if (dn_evt == BTN_EVT_SHORT_PRESS) {
        composer_dur_idx = (composer_dur_idx + 1) % 4;
        soundManager.playNavMove();
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_SHORT_PRESS) {
        // Append note or rest to composer sequence
        if (composer_count < 32) {
            uint16_t freq = getComposerFreq(composer_note_idx, composer_octave);
            uint16_t dur = DUR_MS[composer_dur_idx];
            composer_notes[composer_count++] = { freq, dur };
            if (freq > 0) soundManager.playTone(freq, dur);
            showToast(("[NOTE ADDED #" + String(composer_count) + "]").c_str(), 800);
        }
        needs_redraw = true;
    } else if (ok_evt == BTN_EVT_LONG_PRESS) {
        if (composer_count > 0) {
            // Play built sequence and offer save prompt
            soundManager.playSequence(composer_notes, composer_count);
            openKeyboard("my_tune", "Save Composition", KeyboardMode::ALPHA, false, 20, onComposerSaveEntered);
        }
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
            lab_duty = (lab_duty == 50) ? 75 : ((lab_duty == 75) ? 25 : ((lab_duty == 25) ? 10 : 50));
            soundManager.setToneDuty(2700, lab_duty);
            showToast(("[DUTY " + String(lab_duty) + "% PWM]").c_str(), 1000);
        } else if (sound_selection == 3) {
            soundManager.playQBranch();
            showToast("[BUZZER FREQ CHECK]", 1200);
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

static void onMorseTextEntered(bool success, const String& text) {
    if (success && text.length() > 0) {
        soundManager.playMorse(text);
        ui.showToast(("[MORSE: " + text + "]").c_str(), 1500);
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
        openKeyboard("HELLO", "Morse Text", KeyboardMode::ALPHA, false, 20, onMorseTextEntered);
        needs_redraw = true;
    }
}
