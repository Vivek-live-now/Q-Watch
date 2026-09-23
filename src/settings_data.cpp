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
        else if (key == "raise_to_wake") settings.raise_to_wake = (val == "1");
        else if (key == "wifi_auto_off_idx") settings.wifi_auto_off_idx = val.toInt();
        else if (key == "contrast_idx") settings.contrast_idx = val.toInt();
        else if (key == "low_power") settings.low_power = (val == "1");
        else if (key == "contrast") settings.contrast_idx = val.toInt();
        else if (key == "invert_display") settings.invert_display = (val == "1");
        else if (key == "ui_option_idx") settings.ui_option_idx = val.toInt();
        else if (key == "bme_interval_idx") settings.bme_interval_idx = val.toInt();
        else if (key == "health_bg_enabled") settings.health_bg_enabled = (val == "1");
        else if (key == "health_interval_idx") settings.health_interval_idx = val.toInt();
        else if (key == "sound_master_on") settings.sound_master_on = (val == "1");
        else if (key == "volume_pct") settings.volume_pct = val.toInt();
        else if (key == "button_sounds_on") settings.button_sounds_on = (val == "1");
        else if (key == "notifications_on") settings.notifications_on = (val == "1");
        else if (key == "sound_style_idx") settings.sound_style_idx = val.toInt();
        else if (key == "watch_face_style") settings.watch_face_style = val.toInt();
        else if (key == "show_date") settings.show_date = (val == "1");
        else if (key == "show_battery") settings.show_battery = (val == "1");
        else if (key == "show_weather_widget") settings.show_weather_widget = (val == "1");
        else if (key == "show_steps_widget") settings.show_steps_widget = (val == "1");
        else if (key == "show_status_icons") settings.show_status_icons = (val == "1");
        else if (key == "hourly_chime_enabled") settings.hourly_chime_enabled = (val == "1");
        else if (key == "world_clock_tz_idx") settings.world_clock_tz_idx = val.toInt();
        else if (key == "step_goal") settings.step_goal = (uint32_t)val.toInt();
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
    out += "raise_to_wake=" + String(settings.raise_to_wake ? "1" : "0") + "\n";
    out += "wifi_auto_off_idx=" + String(settings.wifi_auto_off_idx) + "\n";
    out += "contrast_idx=" + String(settings.contrast_idx) + "\n";
    out += "low_power=" + String(settings.low_power ? "1" : "0") + "\n";
    out += "contrast=" + String(settings.contrast_idx) + "\n";
    out += "invert_display=" + String(settings.invert_display ? "1" : "0") + "\n";
    out += "ui_option_idx=" + String(settings.ui_option_idx) + "\n";
    out += "bme_interval_idx=" + String(settings.bme_interval_idx) + "\n";
    out += "health_bg_enabled=" + String(settings.health_bg_enabled ? "1" : "0") + "\n";
    out += "health_interval_idx=" + String(settings.health_interval_idx) + "\n";
    out += "sound_master_on=" + String(settings.sound_master_on ? "1" : "0") + "\n";
    out += "volume_pct=" + String(settings.volume_pct) + "\n";
    out += "button_sounds_on=" + String(settings.button_sounds_on ? "1" : "0") + "\n";
    out += "notifications_on=" + String(settings.notifications_on ? "1" : "0") + "\n";
    out += "sound_style_idx=" + String(settings.sound_style_idx) + "\n";
    out += "watch_face_style=" + String(settings.watch_face_style) + "\n";
    out += "show_date=" + String(settings.show_date ? "1" : "0") + "\n";
    out += "show_battery=" + String(settings.show_battery ? "1" : "0") + "\n";
    out += "show_weather_widget=" + String(settings.show_weather_widget ? "1" : "0") + "\n";
    out += "show_steps_widget=" + String(settings.show_steps_widget ? "1" : "0") + "\n";
    out += "show_status_icons=" + String(settings.show_status_icons ? "1" : "0") + "\n";
    out += "hourly_chime_enabled=" + String(settings.hourly_chime_enabled ? "1" : "0") + "\n";
    out += "world_clock_tz_idx=" + String(settings.world_clock_tz_idx) + "\n";
    out += "step_goal=" + String(settings.step_goal) + "\n";

    fileManager.write("/config/settings", out);
}
