#include "settings_data.h"
#include "file_manager.h"

SettingsManager settingsManager;

SettingsManager::SettingsManager() {}

void SettingsManager::begin() {
    load();
}

void SettingsManager::load() {
    if (!fileManager.exists("/config/settings")) {
        save(); // Write default settings file if missing
        return;
    }

    String content = fileManager.read("/config/settings");
    if (content.length() == 0) return;

    int pos = 0;
    while (pos < content.length()) {
        int next_nl = content.indexOf('\n', pos);
        if (next_nl == -1) next_nl = content.length();
        String line = content.substring(pos, next_nl);
        line.trim();
        pos = next_nl + 1;

        if (line.length() == 0 || line.startsWith("#")) continue;

        int eq = line.indexOf('=');
        if (eq == -1) continue;

        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        key.trim();
        val.trim();

        if (key == "wifi_enabled") settings.wifi_enabled = (val == "1");
        else if (key == "ble_enabled") settings.ble_enabled = (val == "1");
        else if (key == "fileserver_enabled") settings.fileserver_enabled = (val == "1");
        else if (key == "auto_sync") settings.auto_sync = (val == "1");
        else if (key == "timezone_idx") settings.timezone_idx = val.toInt();
        else if (key == "format_24hr") settings.format_24hr = (val == "1");
        else if (key == "display_timeout_idx") settings.display_timeout_idx = val.toInt();
        else if (key == "sleep_time_idx") settings.sleep_time_idx = val.toInt();
        else if (key == "low_power") settings.low_power = (val == "1");
        else if (key == "contrast") settings.contrast = val.toInt();
        else if (key == "invert_display") settings.invert_display = (val == "1");
        else if (key == "ui_option_idx") settings.ui_option_idx = val.toInt();
    }
}

void SettingsManager::save() {
    String out = "";
    out += "wifi_enabled=" + String(settings.wifi_enabled ? "1" : "0") + "\n";
    out += "ble_enabled=" + String(settings.ble_enabled ? "1" : "0") + "\n";
    out += "fileserver_enabled=" + String(settings.fileserver_enabled ? "1" : "0") + "\n";
    out += "auto_sync=" + String(settings.auto_sync ? "1" : "0") + "\n";
    out += "timezone_idx=" + String(settings.timezone_idx) + "\n";
    out += "format_24hr=" + String(settings.format_24hr ? "1" : "0") + "\n";
    out += "display_timeout_idx=" + String(settings.display_timeout_idx) + "\n";
    out += "sleep_time_idx=" + String(settings.sleep_time_idx) + "\n";
    out += "low_power=" + String(settings.low_power ? "1" : "0") + "\n";
    out += "contrast=" + String(settings.contrast) + "\n";
    out += "invert_display=" + String(settings.invert_display ? "1" : "0") + "\n";
    out += "ui_option_idx=" + String(settings.ui_option_idx) + "\n";

    // Create /config directory if needed by creating file directly
    if (!fileManager.exists("/config")) {
        // fileManager automatically normalizes path, write creates file
    }
    fileManager.write("/config/settings", out);
}
