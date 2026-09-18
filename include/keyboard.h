#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <Arduino.h>

enum class KeyboardMode {
    ALPHA,      // QWERTY / Alpha-numeric
    NUMERIC,    // Numpad 0-9, dot
    HEX_MODE         // Hex 0-9, A-F
};

enum class KeyboardResult {
    EDITING,
    CONFIRMED,
    CANCELLED
};

class KeyboardManager {
public:
    KeyboardManager();

    void open(const String& initial_text, const String& title, KeyboardMode mode = KeyboardMode::ALPHA, bool mask_input = false, int max_len = 32);
    void handleInput();

    bool isActive() const { return active; }
    KeyboardResult getResult() const { return result; }
    String getText() const { return text_buffer; }
    String getTitle() const { return title; }
    bool isMasked() const { return mask_input; }

    KeyboardMode getMode() const { return mode; }
    int getSelectedRow() const { return cursor_row; }
    int getSelectedCol() const { return cursor_col; }
    bool isCaps() const { return caps; }

    // Grid details
    int getRowCount() const;
    int getColCount(int row) const;
    const char* getKeyLabel(int row, int col) const;

private:
    bool active;
    KeyboardResult result;
    KeyboardMode mode;
    String text_buffer;
    String title;
    bool mask_input;
    int max_len;

    int cursor_row;
    int cursor_col;
    bool caps;

    void processKeyAction(int row, int col);
};

extern KeyboardManager keyboardManager;

#endif
