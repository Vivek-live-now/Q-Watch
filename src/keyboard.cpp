#include "keyboard.h"
#include "button_manager.h"
#include "sound_manager.h"

KeyboardManager keyboardManager;

// 3-row layout for 128x64 OLED
// Row 0: Alpha / Numbers
// Row 1: Symbols / Letters
// Row 2: Control keys (OK, CAP, DEL, SPC, CANCEL)

static const char* const ALPHA_ROW0_LOWER[] = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
static const char* const ALPHA_ROW0_UPPER[] = {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"};

static const char* const ALPHA_ROW1_LOWER[] = {"a", "s", "d", "f", "g", "h", "j", "k", "l", "@"};
static const char* const ALPHA_ROW1_UPPER[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L", "_"};

static const char* const ALPHA_ROW2_LOWER[] = {"z", "x", "c", "v", "b", "n", "m", ".", "-", "/"};
static const char* const ALPHA_ROW2_UPPER[] = {"Z", "X", "C", "V", "B", "N", "M", "1", "2", "3"};

static const char* const ALPHA_ROW3_CTRL[]  = {"OK", "CAP", "DEL", "SPC", "ESC"};

// Numeric layout
static const char* const NUM_ROW0[] = {"1", "2", "3", "4", "5"};
static const char* const NUM_ROW1[] = {"6", "7", "8", "9", "0"};
static const char* const NUM_ROW2[] = {".", "-", "+", "DEL", "OK"};

// Hex layout
static const char* const HEX_MODE_ROW0[] = {"0", "1", "2", "3", "4", "5", "6", "7"};
static const char* const HEX_MODE_ROW1[] = {"8", "9", "A", "B", "C", "D", "E", "F"};
static const char* const HEX_MODE_ROW2[] = {"DEL", "SPC", "CLR", "ESC", "OK"};

KeyboardManager::KeyboardManager() :
    active(false),
    result(KeyboardResult::EDITING),
    mode(KeyboardMode::ALPHA),
    text_buffer(""),
    title(""),
    mask_input(false),
    max_len(32),
    cursor_row(0),
    cursor_col(0),
    caps(false) {}

void KeyboardManager::open(const String& initial_text, const String& t, KeyboardMode m, bool mask, int len) {
    text_buffer = initial_text;
    title = t;
    mode = m;
    mask_input = mask;
    max_len = len;
    active = true;
    result = KeyboardResult::EDITING;
    cursor_row = 0;
    cursor_col = 0;
    caps = false;
}

int KeyboardManager::getRowCount() const {
    if (mode == KeyboardMode::ALPHA) return 4;
    return 3;
}

int KeyboardManager::getColCount(int row) const {
    if (mode == KeyboardMode::ALPHA) {
        if (row == 3) return 5;
        return 10;
    } else if (mode == KeyboardMode::NUMERIC) {
        return 5;
    } else if (mode == KeyboardMode::HEX_MODE) {
        if (row == 2) return 5;
        return 8;
    }
    return 0;
}

const char* KeyboardManager::getKeyLabel(int row, int col) const {
    if (mode == KeyboardMode::ALPHA) {
        if (row == 0) return caps ? ALPHA_ROW0_UPPER[col] : ALPHA_ROW0_LOWER[col];
        if (row == 1) return caps ? ALPHA_ROW1_UPPER[col] : ALPHA_ROW1_LOWER[col];
        if (row == 2) return caps ? ALPHA_ROW2_UPPER[col] : ALPHA_ROW2_LOWER[col];
        if (row == 3) return ALPHA_ROW3_CTRL[col];
    } else if (mode == KeyboardMode::NUMERIC) {
        if (row == 0) return NUM_ROW0[col];
        if (row == 1) return NUM_ROW1[col];
        if (row == 2) return NUM_ROW2[col];
    } else if (mode == KeyboardMode::HEX_MODE) {
        if (row == 0) return HEX_MODE_ROW0[col];
        if (row == 1) return HEX_MODE_ROW1[col];
        if (row == 2) return HEX_MODE_ROW2[col];
    }
    return "";
}

void KeyboardManager::handleInput() {
    if (!active) return;

    ButtonEvent up_evt = btnManager.getEvent(BTN_ID_UP);
    ButtonEvent dn_evt = btnManager.getEvent(BTN_ID_DN);
    ButtonEvent sel_evt = btnManager.getEvent(BTN_ID_SEL);

    // UP button short = previous col; repeat/long = previous row
    if (up_evt == BTN_EVT_SHORT_PRESS) {
        cursor_col--;
        if (cursor_col < 0) {
            cursor_row--;
            if (cursor_row < 0) cursor_row = getRowCount() - 1;
            cursor_col = getColCount(cursor_row) - 1;
        }
        soundManager.playNavMove();
    } else if (up_evt == BTN_EVT_REPEAT || up_evt == BTN_EVT_LONG_PRESS) {
        cursor_row--;
        if (cursor_row < 0) cursor_row = getRowCount() - 1;
        if (cursor_col >= getColCount(cursor_row)) cursor_col = getColCount(cursor_row) - 1;
        soundManager.playNavMove();
    }

    // DOWN button short = next col; repeat/long = next row
    if (dn_evt == BTN_EVT_SHORT_PRESS) {
        cursor_col++;
        if (cursor_col >= getColCount(cursor_row)) {
            cursor_col = 0;
            cursor_row++;
            if (cursor_row >= getRowCount()) cursor_row = 0;
        }
        soundManager.playNavMove();
    } else if (dn_evt == BTN_EVT_REPEAT || dn_evt == BTN_EVT_LONG_PRESS) {
        cursor_row++;
        if (cursor_row >= getRowCount()) cursor_row = 0;
        if (cursor_col >= getColCount(cursor_row)) cursor_col = getColCount(cursor_row) - 1;
        soundManager.playNavMove();
    }

    // SELECT short = trigger key action; long = backspace / ESC
    if (sel_evt == BTN_EVT_SHORT_PRESS) {
        soundManager.playNavSelect();
        processKeyAction(cursor_row, cursor_col);
    } else if (sel_evt == BTN_EVT_LONG_PRESS) {
        soundManager.playNavBack();
        if (text_buffer.length() > 0) {
            text_buffer.remove(text_buffer.length() - 1);
        } else {
            result = KeyboardResult::CANCELLED;
            active = false;
        }
    }
}

void KeyboardManager::processKeyAction(int row, int col) {
    const char* label = getKeyLabel(row, col);

    if (strcmp(label, "OK") == 0) {
        result = KeyboardResult::CONFIRMED;
        active = false;
    } else if (strcmp(label, "CAP") == 0) {
        caps = !caps;
    } else if (strcmp(label, "DEL") == 0) {
        if (text_buffer.length() > 0) {
            text_buffer.remove(text_buffer.length() - 1);
        }
    } else if (strcmp(label, "CLR") == 0) {
        text_buffer = "";
    } else if (strcmp(label, "SPC") == 0) {
        if (text_buffer.length() < (size_t)max_len) {
            text_buffer += " ";
        }
    } else if (strcmp(label, "ESC") == 0) {
        result = KeyboardResult::CANCELLED;
        active = false;
    } else {
        if (text_buffer.length() < (size_t)max_len && label[0] != '\0') {
            text_buffer += label;
        }
    }
}
