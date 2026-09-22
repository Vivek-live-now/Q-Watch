with open('src/ui_core.cpp', 'r') as f:
    content = f.read()

old_code = '''static void onQuickRemoteNameEntered(bool success, const String& name) {
    if (success && name.length() > 0) {
        ui.showToast("[REMOTE CREATED]", 1200);
        ui.setIrSubmenu(IrSubmenu::QUICK_REMOTE_BUILD);
    }
}

void UICore::handleQuickRemoteInput() {
    ButtonEvent ok_evt = btnManager.getEvent(BTN_ID_OK);

    if (ir_submenu == IrSubmenu::QUICK_REMOTE) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            openKeyboard("CustomRemote", "Remote Name", KeyboardMode::ALPHA, false, 24, onQuickRemoteNameEntered);
        }
    } else if (ir_submenu == IrSubmenu::QUICK_REMOTE_BUILD) {
        if (ok_evt == BTN_EVT_SHORT_PRESS) {
            soundManager.playNavSelect();
            irEngine.startCapture();
            ir_submenu = IrSubmenu::QUICK_REMOTE_WAIT;
            needs_redraw = true;
        }
    }
}'''

new_code = '''static String pending_qr_remote_name = "";

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
}'''

if old_code in content:
    content = content.replace(old_code, new_code)
    with open('src/ui_core.cpp', 'w') as f:
        f.write(content)
    print("Updated handleQuickRemoteInput successfully.")
else:
    print("Could not find old_code.")
