with open('src/display.cpp', 'r') as f:
    content = f.read()

ir_render_code = '''
void DisplayManager::drawAppIR() {
    drawTopStatusBar();
    IrSubmenu sub = ui.getIrSubmenu();

    switch (sub) {
        case IrSubmenu::MAIN: {
            drawMenu("IR REMOTE", ui.ir_main_items, UICore::IR_MAIN_ITEM_COUNT);
            break;
        }
        case IrSubmenu::TV_B_GONE: {
            oled.setFont(u8g2_font_5x7_tr);
            oled.drawStr(2, 17, "TV-B-GONE POWER BLAST");
            oled.drawLine(0, 19, 128, 19);

            oled.setFont(u8g2_font_6x10_tr);
            if (irEngine.isTvBGoneRunning()) {
                oled.drawStr(4, 32, "STATUS: RUNNING...");
                char buf[32];
                snprintf(buf, sizeof(buf), "CODE %d/%d (%s)",
                         irEngine.getTvBGoneCurrentIndex(),
                         irEngine.getTvBGoneTotalCount(),
                         irEngine.getTvBGoneCurrentBrand().c_str());
                oled.drawStr(4, 44, buf);

                oled.drawFrame(4, 50, 120, 8);
                int pct = irEngine.getTvBGoneProgressPercent();
                if (pct > 0) oled.drawBox(4, 50, (pct * 120) / 100, 8);
            } else {
                oled.drawStr(4, 32, "STATUS: READY / IDLE");
                oled.drawStr(4, 46, "[OK]: START BLAST");
            }
            break;
        }
        case IrSubmenu::CUSTOM_IR: {
            drawMenu("CUSTOM IR", ui.ir_custom_items, UICore::IR_CUSTOM_ITEM_COUNT);
            break;
        }
        case IrSubmenu::REMOTE_VIEW: {
            const IrRemoteFile& rem = ui.getIrActiveRemote();
            oled.setFont(u8g2_font_5x7_tr);
            String title = rem.name.length() > 0 ? rem.name : "IR REMOTE";
            oled.drawStr(2, 17, title.c_str());
            oled.drawLine(0, 19, 128, 19);

            oled.setFont(u8g2_font_6x10_tr);
            int count = rem.buttons.size();
            if (count == 0) {
                oled.drawStr(10, 36, "(NO BUTTONS)");
                break;
            }

            int sel = ui.getIrSelection();
            int offset = ui.getIrScrollOffset();
            int y_pos = 30;

            for (int i = offset; i < offset + 3 && i < count; i++) {
                if (i == sel) {
                    oled.drawBox(2, y_pos - 8, 118, 10);
                    oled.setDrawColor(0);
                    oled.drawStr(4, y_pos, rem.buttons[i].name.c_str());
                    String tStr = (rem.buttons[i].type == IrSignalType::PARSED) ? rem.buttons[i].protocol : "RAW";
                    int tw = oled.getStrWidth(tStr.c_str());
                    oled.drawStr(118 - tw, y_pos, tStr.c_str());
                    oled.setDrawColor(1);
                } else {
                    oled.drawStr(4, y_pos, rem.buttons[i].name.c_str());
                    String tStr = (rem.buttons[i].type == IrSignalType::PARSED) ? rem.buttons[i].protocol : "RAW";
                    int tw = oled.getStrWidth(tStr.c_str());
                    oled.drawStr(118 - tw, y_pos, tStr.c_str());
                }
                y_pos += 12;
            }

            if (count > 3) {
                int scroll_h = 30;
                int scroll_y = 22 + ((float)offset / (count - 3)) * (scroll_h - 10);
                oled.drawFrame(123, 22, 3, 30);
                oled.drawBox(123, scroll_y, 3, 10);
            }
            break;
        }
        case IrSubmenu::IR_READ:
        case IrSubmenu::IR_READ_WAIT:
        case IrSubmenu::IR_READ_RESULT: {
            oled.setFont(u8g2_font_5x7_tr);
            oled.drawStr(2, 17, "IR READ / DECODE");
            oled.drawLine(0, 19, 128, 19);

            oled.setFont(u8g2_font_6x10_tr);
            if (sub == IrSubmenu::IR_READ) {
                oled.drawStr(4, 34, "POINT REMOTE AT WATCH");
                oled.drawStr(4, 48, "[OK]: START LEARN");
            } else if (sub == IrSubmenu::IR_READ_WAIT) {
                oled.drawStr(4, 38, "LISTENING FOR IR...");
            } else {
                const IrButton& b = ui.getIrCapturedButton();
                if (b.type == IrSignalType::PARSED) {
                    oled.drawStr(4, 28, ("PROTO: " + b.protocol).c_str());
                    char buf[32];
                    snprintf(buf, sizeof(buf), "CMD  : 0x%04X", (uint32_t)b.command);
                    oled.drawStr(4, 40, buf);
                } else {
                    oled.drawStr(4, 28, "PROTO: UNKNOWN (RAW)");
                    char buf[32];
                    snprintf(buf, sizeof(buf), "PULSES: %d", (int)b.raw_data.size());
                    oled.drawStr(4, 40, buf);
                }
                oled.drawStr(4, 54, "[OK]: TEST TRANSMIT");
            }
            break;
        }
        case IrSubmenu::QUICK_REMOTE:
        case IrSubmenu::QUICK_REMOTE_BUILD:
        case IrSubmenu::QUICK_REMOTE_WAIT: {
            oled.setFont(u8g2_font_5x7_tr);
            oled.drawStr(2, 17, "QUICK REMOTE SETUP");
            oled.drawLine(0, 19, 128, 19);

            oled.setFont(u8g2_font_6x10_tr);
            if (sub == IrSubmenu::QUICK_REMOTE) {
                oled.drawStr(4, 34, "CREATE VIRTUAL REMOTE");
                oled.drawStr(4, 48, "[OK]: NAME REMOTE");
            } else if (sub == IrSubmenu::QUICK_REMOTE_BUILD) {
                const IrRemoteFile& rem = ui.getIrActiveRemote();
                char buf[32];
                snprintf(buf, sizeof(buf), "REMOTE: %s", rem.name.c_str());
                oled.drawStr(4, 28, buf);
                snprintf(buf, sizeof(buf), "BUTTONS: %d", (int)rem.buttons.size());
                oled.drawStr(4, 40, buf);
                oled.drawStr(4, 54, "[OK]: ADD BUTTON");
            } else {
                oled.drawStr(4, 34, "POINT REMOTE & PRESS");
                oled.drawStr(4, 48, "[WAITING SIGNAL...]");
            }
            break;
        }
        case IrSubmenu::UNIVERSAL: {
            oled.setFont(u8g2_font_5x7_tr);
            oled.drawStr(2, 17, "UNIVERSAL REMOTES");
            oled.drawLine(0, 19, 128, 19);
            oled.setFont(u8g2_font_6x10_tr);
            oled.drawStr(4, 32, "CATEGORIES: TV / AC");
            oled.drawStr(4, 48, "[OK]: SEARCH CODES");
            break;
        }
        case IrSubmenu::RECENT: {
            oled.setFont(u8g2_font_5x7_tr);
            oled.drawStr(2, 17, "RECENT IR SIGNALS");
            oled.drawLine(0, 19, 128, 19);

            std::vector<String> list = irEngine.getRecentList();
            int count = list.size();
            oled.setFont(u8g2_font_6x10_tr);
            if (count == 0) {
                oled.drawStr(10, 36, "(NO RECENT SIGNALS)");
                break;
            }

            int sel = ui.getIrSelection();
            int offset = ui.getIrScrollOffset();
            int y_pos = 30;

            for (int i = offset; i < offset + 3 && i < count; i++) {
                if (i == sel) {
                    oled.drawBox(2, y_pos - 8, 118, 10);
                    oled.setDrawColor(0);
                    oled.drawStr(4, y_pos, list[i].c_str());
                    oled.setDrawColor(1);
                } else {
                    oled.drawStr(4, y_pos, list[i].c_str());
                }
                y_pos += 12;
            }
            break;
        }
        case IrSubmenu::FAVORITES: {
            oled.setFont(u8g2_font_5x7_tr);
            oled.drawStr(2, 17, "FAVORITE IR BUTTONS");
            oled.drawLine(0, 19, 128, 19);

            std::vector<String> list = irEngine.getFavoritesList();
            int count = list.size();
            oled.setFont(u8g2_font_6x10_tr);
            if (count == 0) {
                oled.drawStr(10, 36, "(NO FAVORITES)");
                break;
            }

            int sel = ui.getIrSelection();
            int offset = ui.getIrScrollOffset();
            int y_pos = 30;

            for (int i = offset; i < offset + 3 && i < count; i++) {
                if (i == sel) {
                    oled.drawBox(2, y_pos - 8, 118, 10);
                    oled.setDrawColor(0);
                    oled.drawStr(4, y_pos, list[i].c_str());
                    oled.setDrawColor(1);
                } else {
                    oled.drawStr(4, y_pos, list[i].c_str());
                }
                y_pos += 12;
            }
            break;
        }
        case IrSubmenu::IR_FILES: {
            oled.setFont(u8g2_font_5x7_tr);
            oled.drawStr(2, 17, "IR FILES (/ir)");
            oled.drawLine(0, 19, 128, 19);

            std::vector<String> list = irEngine.listIrFiles();
            int count = list.size();
            oled.setFont(u8g2_font_6x10_tr);
            if (count == 0) {
                oled.drawStr(10, 36, "(NO .IR FILES)");
                break;
            }

            int sel = ui.getIrSelection();
            int offset = ui.getIrScrollOffset();
            int y_pos = 30;

            for (int i = offset; i < offset + 3 && i < count; i++) {
                String fname = list[i];
                int slash = fname.lastIndexOf('/');
                if (slash >= 0) fname = fname.substring(slash + 1);

                if (i == sel) {
                    oled.drawBox(2, y_pos - 8, 118, 10);
                    oled.setDrawColor(0);
                    oled.drawStr(4, y_pos, fname.c_str());
                    oled.setDrawColor(1);
                } else {
                    oled.drawStr(4, y_pos, fname.c_str());
                }
                y_pos += 12;
            }
            break;
        }
        case IrSubmenu::IR_LAB: {
            drawMenu("IR SIGNAL LAB", ui.ir_lab_items, UICore::IR_LAB_ITEM_COUNT);
            if (irEngine.isCarrierTestActive()) {
                oled.setFont(u8g2_font_4x6_tr);
                char buf[32];
                snprintf(buf, sizeof(buf), "[CARRIER %d Hz ON]", (int)irEngine.getCarrierFreq());
                oled.drawStr(4, 62, buf);
            }
            break;
        }
    }
}
'''

old_fn = '''void DisplayManager::drawAppIR() {
    oled.setFont(u8g2_font_6x10_tr);
    oled.drawStr(10, 28, "EMITTER: ARMED");
    oled.drawStr(10, 42, "SENSOR : STANDBY");
}'''

if old_fn in content:
    content = content.replace(old_fn, ir_render_code)
    with open('src/display.cpp', 'w') as f:
        f.write(content)
    print("Replaced drawAppIR successfully.")
else:
    print("Could not find old drawAppIR.")
