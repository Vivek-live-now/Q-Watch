#include "qlink.h"

#ifdef ARDUINO
#include "display.h"
#include "button_manager.h"
#include "sensors.h"
#include "battery.h"
#include "max30102_manager.h"
#include "sound_manager.h"
#include "led_manager.h"
#include "ui_core.h"
#include "timekeeping.h"
#include "clock.h"
#include "file_manager.h"
#include "weather.h"
#include "config.h"
#include "mochi_pet.h"
#include "power_manager.h"
#include "wireless_recon.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <ArduinoJson.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

QLinkEngine qlink;

QLinkEngine::QLinkEngine() :
    packet_seq(0),
    display_streaming(false),
    target_stream_fps(20),
    last_stream_frame_time(0)
{}

void QLinkEngine::begin() {
    packet_seq = 0;
    display_streaming = false;
}

void QLinkEngine::loop() {
    // Background streaming cadence & telemetry heartbeat
}

void QLinkEngine::setStreamingDisplay(bool enable, uint8_t target_fps) {
    display_streaming = enable;
    if (target_fps > 0 && target_fps <= 50) {
        target_stream_fps = target_fps;
    }
}

void QLinkEngine::fillCompactTelemetry(QLinkCompactTelemetry& out) {
    out.magic = QLINK_MAGIC;
    out.seq = packet_seq++;

#ifdef ARDUINO
    out.battery_pct = (uint8_t)constrain(battery.readPercentage(), 0, 100);
    out.battery_mv = (uint16_t)(battery.readVoltage() * 1000.0f);

    EnvironmentData env = sensors.getEnvData();
    out.temp_c_x10 = (int16_t)(env.temperature * 10.0f);
    out.pressure_hpa_x10 = (uint16_t)(env.pressure * 10.0f);
    out.humidity_pct_x10 = (uint16_t)(env.humidity * 10.0f);
    out.altitude_m = (int16_t)env.altitude;

    OrientationData ori = sensors.getOrientation();
    out.pitch_deg_x10 = (int16_t)(ori.pitch * 10.0f);
    out.roll_deg_x10 = (int16_t)(ori.roll * 10.0f);
    out.heading_deg_x10 = (uint16_t)(ori.yaw * 10.0f);

    HealthMetrics hm = max30102Manager.getMetrics();
    out.heart_rate_bpm = (uint8_t)constrain(hm.bpm, 0, 255);
    out.spo2_pct = (uint8_t)constrain(hm.spo2, 0, 100);

    out.step_count = timekeeping.pedometer.getSteps();

    uint16_t flags = 0;
    if (WiFi.status() == WL_CONNECTED) flags |= (1 << 1);
    if (ui.getState() == UIState::SLEEPING) flags |= (1 << 2);
    out.status_flags = flags;

    out.uptime_sec = millis() / 1000;
#else
    out.battery_pct = 85;
    out.battery_mv = 3950;
    out.temp_c_x10 = 245;
    out.pressure_hpa_x10 = 10132;
    out.humidity_pct_x10 = 550;
    out.altitude_m = 920;
    out.pitch_deg_x10 = 15;
    out.roll_deg_x10 = -8;
    out.heading_deg_x10 = 1800;
    out.heart_rate_bpm = 72;
    out.spo2_pct = 98;
    out.step_count = 5420;
    out.status_flags = 0;
    out.uptime_sec = 120;
#endif
}

String QLinkEngine::generateTelemetryJson() {
    String json;
    json.reserve(512);
#ifdef ARDUINO
    EnvironmentData env = sensors.getEnvData();
    OrientationData ori = sensors.getOrientation();
    HealthMetrics hm = max30102Manager.getMetrics();

    json = "{\"bme280\":{";
    json += "\"temperature_c\":" + String(env.temperature, 2) + ",";
    json += "\"pressure_hpa\":" + String(env.pressure, 2) + ",";
    json += "\"humidity_pct\":" + String(env.humidity, 1) + ",";
    json += "\"altitude_m\":" + String(env.altitude, 1) + "},";

    json += "\"imu\":{";
    json += "\"pitch_deg\":" + String(ori.pitch, 1) + ",";
    json += "\"roll_deg\":" + String(ori.roll, 1) + ",";
    json += "\"heading_deg\":" + String(ori.yaw, 1) + "},";

    json += "\"health\":{";
    json += "\"heart_rate_bpm\":" + String(hm.bpm) + ",";
    json += "\"spo2_pct\":" + String(hm.spo2) + ",";
    json += "\"finger_detected\":" + String(hm.finger_detected ? "true" : "false") + "},";

    json += "\"pedometer\":{";
    json += "\"steps\":" + String(timekeeping.pedometer.getSteps()) + ",";
    json += "\"distance_km\":" + String(timekeeping.pedometer.getDistanceKm(), 2) + ",";
    json += "\"calories_kcal\":" + String(timekeeping.pedometer.getCaloriesKcal()) + "},";

    json += "\"battery\":{";
    json += "\"percent\":" + String(battery.readPercentage()) + ",";
    json += "\"voltage_v\":" + String(battery.readVoltage(), 2) + "}}";
#else
    json = "{\"status\":\"mock\"}";
#endif
    return json;
}

String QLinkEngine::generateDeviceInfoJson() {
    String json;
    json.reserve(512);
#ifdef ARDUINO
    json = "{\"device\":\"Q-Watch\",\"model\":\"ESP32-S3-SuperMini\",";
    json += "\"firmware_version\":\"1.5.0\",";
    json += "\"protocol_version\":\"" QLINK_PROTOCOL_VERSION "\",";
    json += "\"mac\":\"" + WiFi.macAddress() + "\",";
    String current_ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    json += "\"ip\":\"" + current_ip + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"battery\":{\"percent\":" + String(battery.readPercentage()) + ",";
    json += "\"voltage_mv\":" + String((int)(battery.readVoltage() * 1000.0f)) + "},";
    json += "\"memory\":{\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"fs_total_bytes\":" + String(LittleFS.totalBytes()) + ",";
    json += "\"fs_used_bytes\":" + String(LittleFS.usedBytes()) + "},";
    json += "\"uptime_sec\":" + String(millis() / 1000) + "}";
#else
    json = "{\"device\":\"Q-Watch\",\"firmware_version\":\"1.5.0\",\"protocol_version\":\"" QLINK_PROTOCOL_VERSION "\"}";
#endif
    return json;
}

bool QLinkEngine::injectButton(const String& btn_str, const String& evt_str) {
    int id = -1;
    String b = btn_str;
    b.toUpperCase();
    if (b == "UP") id = 0;
    else if (b == "OK" || b == "SEL" || b == "SELECT") id = 1;
    else if (b == "DN" || b == "DOWN") id = 2;
    else if (b == "CANCEL" || b == "BACK") id = 3;
    else return false;

    int evt = 1; // SHORT_PRESS
    String e = evt_str;
    e.toUpperCase();
    if (e.indexOf("LONG") >= 0) evt = 2; // LONG_PRESS
    else if (e.indexOf("DOUBLE") >= 0) evt = 4; // DOUBLE_TAP
    else evt = 1;

#ifdef ARDUINO
    btnManager.injectEvent((ButtonID)id, (ButtonEvent)evt);
#endif
    return true;
}

#ifdef ARDUINO
static CRGB parseHexColor(const String& hex) {
    String clean = hex;
    clean.trim();
    if (clean.startsWith("#")) clean = clean.substring(1);
    if (clean.length() == 6) {
        long val = strtol(clean.c_str(), nullptr, 16);
        return CRGB((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
    }
    return CRGB::Cyan;
}
#endif

void QLinkEngine::handleNotification(const String& app_name, const String& title, const String& body,
                                    const String& alert_style, const String& led_hex) {
#ifdef ARDUINO
    String toast = app_name;
    if (title.length() > 0) toast += ": " + title;
    else if (body.length() > 0) toast += ": " + body;
    ui.showToast(toast.c_str(), 2500);

    if (alert_style != "SILENT") {
        soundManager.playNotification();
    }
    CRGB color = parseHexColor(led_hex);
    ledManager.triggerPulse(color, 2, 80);
#endif
}

bool QLinkEngine::syncTime(uint32_t epoch_sec, int32_t tz_offset_sec) {
#ifdef ARDUINO
    (void)tz_offset_sec;
    qclock.setEpoch(epoch_sec);
    return true;
#else
    (void)epoch_sec;
    (void)tz_offset_sec;
    return true;
#endif
}

bool QLinkEngine::syncWeather(const String& city, float temp_c, int humidity, int code, const String& desc, float high_c, float low_c) {
#ifdef ARDUINO
    (void)city;
    (void)code;
    (void)high_c;
    (void)low_c;
    WeatherData wd;
    wd.temperature = temp_c;
    wd.feels_like = temp_c;
    wd.humidity = humidity;
    wd.wind_speed = 0.0f;
    wd.condition = desc;
    wd.valid = true;
    weather.setManualWeather(wd);
    return true;
#else
    (void)city;
    (void)temp_c;
    (void)humidity;
    (void)code;
    (void)desc;
    (void)high_c;
    (void)low_c;
    return true;
#endif
}

bool QLinkEngine::validateQAppHeader(const uint8_t* data, size_t len, String& out_app_name, String& out_version, size_t& out_size, String* out_error) {
    if (!data || len < sizeof(QAppFileHeader)) {
        if (out_error) *out_error = "Payload smaller than QAppFileHeader";
        return false;
    }
    const QAppFileHeader* hdr = (const QAppFileHeader*)data;
    if (hdr->magic != QAPP_MAGIC) {
        if (out_error) *out_error = "Invalid QAPP magic header";
        return false;
    }
    if (hdr->api_version > QAPP_API_VERSION) {
        if (out_error) *out_error = "Incompatible QAPP API version";
        return false;
    }
    if (hdr->code_offset + hdr->code_size > len) {
        if (out_error) *out_error = "Code section out of bounds";
        return false;
    }
    if (hdr->data_offset + hdr->data_size > len) {
        if (out_error) *out_error = "Data section out of bounds";
        return false;
    }
    if (hdr->reloc_offset + hdr->reloc_count * sizeof(QAppReloc) > len) {
        if (out_error) *out_error = "Relocation table out of bounds";
        return false;
    }
    out_app_name = hdr->name;
    out_version = hdr->version;
    out_size = len;
    return true;
}

#ifdef ARDUINO
void QLinkEngine::registerHttpRoutes(WebServer& server) {
    // 1. Device Info
    server.on("/api/v1/info", HTTP_GET, [&server, this]() {
        server.send(200, "application/json", generateDeviceInfoJson());
    });

    // 2. Telemetry
    server.on("/api/v1/telemetry", HTTP_GET, [&server, this]() {
        server.send(200, "application/json", generateTelemetryJson());
    });

    // 3. Display Frame Buffer (1024 bytes raw binary)
    server.on("/api/v1/display/frame", HTTP_GET, [&server]() {
        const uint8_t* buf = displayManager.getBufferPtr();
        server.send_P(200, "application/octet-stream", (const char*)buf, 1024);
    });

    // 4. Display Streaming Control
    server.on("/api/v1/display/control", HTTP_POST, [&server, this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
                bool str = doc["streaming"] | true;
                int fps = doc["fps"] | 20;
                setStreamingDisplay(str, fps);
                server.send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
        }
        server.send(400, "application/json", "{\"error\":\"bad_json\"}");
    });

    // 5. Button Event Injection
    server.on("/api/v1/button", HTTP_POST, [&server, this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
                String btn = doc["button"] | "";
                String evt = doc["event"] | "SHORT_PRESS";
                if (injectButton(btn, evt)) {
                    server.send(200, "application/json", "{\"status\":\"ok\"}");
                    return;
                }
            }
        }
        server.send(400, "application/json", "{\"error\":\"invalid_button\"}");
    });

    // 6. Notification Ingestion
    server.on("/api/v1/notification", HTTP_POST, [&server, this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
                String app = doc["app_name"] | "Phone";
                String title = doc["title"] | "";
                String body = doc["body"] | "";
                String alert = doc["alert_style"] | "CHIME";
                String led = doc["led_color"] | "#00E5FF";
                handleNotification(app, title, body, alert, led);
                server.send(200, "application/json", "{\"status\":\"received\"}");
                return;
            }
        }
        server.send(400, "application/json", "{\"error\":\"bad_notification\"}");
    });

    // 7. Time Synchronization
    server.on("/api/v1/sync/time", HTTP_POST, [&server, this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
                uint32_t epoch = doc["epoch_sec"] | 0;
                int32_t offset = doc["timezone_offset_sec"] | 0;
                if (epoch > 0) {
                    syncTime(epoch, offset);
                    server.send(200, "application/json", "{\"status\":\"synced\"}");
                    return;
                }
            }
        }
        server.send(400, "application/json", "{\"error\":\"invalid_epoch\"}");
    });

    // 8. Weather Synchronization
    server.on("/api/v1/sync/weather", HTTP_POST, [&server, this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
                String city = doc["city"] | "";
                float temp = doc["temp_c"] | 25.0f;
                int hum = doc["humidity_pct"] | 50;
                int code = doc["condition_code"] | 800;
                String desc = doc["condition_str"] | "Clear";
                float hi = doc["high_c"] | temp;
                float lo = doc["low_c"] | temp;
                syncWeather(city, temp, hum, code, desc, hi, lo);
                server.send(200, "application/json", "{\"status\":\"synced\"}");
                return;
            }
        }
        server.send(400, "application/json", "{\"error\":\"invalid_weather\"}");
    });

    // 9. Filesystem Directory List
    server.on("/api/v1/fs/list", HTTP_GET, [&server]() {
        String path = server.hasArg("path") ? server.arg("path") : "/";
        if (!FileManager::isPathSafe(path)) {
            server.send(400, "application/json", "{\"error\":\"invalid_path\"}");
            return;
        }
        FileInfo files[32];
        size_t count = fileManager.listDir(path, files, 32);
        String json = "{\"path\":\"" + path + "\",\"files\":[";
        for (size_t i = 0; i < count; i++) {
            if (i > 0) json += ",";
            json += "{\"name\":\"" + files[i].name + "\",\"size\":" + String((unsigned long)files[i].size) + ",\"is_dir\":" + (files[i].isDir ? "true" : "false") + "}";
        }
        json += "]}";
        server.send(200, "application/json", json);
    });

    // 10. Filesystem Download
    server.on("/api/v1/fs/download", HTTP_GET, [&server]() {
        if (!server.hasArg("path")) {
            server.send(400, "application/json", "{\"error\":\"missing_path\"}");
            return;
        }
        String path = server.arg("path");
        if (!FileManager::isPathSafe(path) || !fileManager.exists(path)) {
            server.send(404, "application/json", "{\"error\":\"not_found\"}");
            return;
        }
        File f = LittleFS.open(path, "r");
        if (!f) {
            server.send(500, "application/json", "{\"error\":\"open_failed\"}");
            return;
        }
        server.streamFile(f, "application/octet-stream");
        f.close();
    });

    // 11. Filesystem Delete
    auto handleDelete = [&server]() {
        if (!server.hasArg("path")) {
            server.send(400, "application/json", "{\"error\":\"missing_path\"}");
            return;
        }
        String path = server.arg("path");
        if (!FileManager::isPathSafe(path)) {
            server.send(400, "application/json", "{\"error\":\"invalid_path\"}");
            return;
        }
        if (fileManager.remove(path)) {
            server.send(200, "application/json", "{\"status\":\"deleted\"}");
        } else {
            server.send(404, "application/json", "{\"error\":\"delete_failed\"}");
        }
    };
    server.on("/api/v1/fs/delete", HTTP_DELETE, handleDelete);
    server.on("/api/v1/fs/delete", HTTP_POST, handleDelete);

    // 12. Micro-ELF Q-App Sideload / Install
    server.on("/api/v1/app/install", HTTP_POST, [&server, this]() {
        if (!server.hasArg("plain")) {
            server.send(400, "application/json", "{\"error\":\"missing_payload\"}");
            return;
        }
        const String& body = server.arg("plain");
        const uint8_t* data = (const uint8_t*)body.c_str();
        size_t len = body.length();

        String app_name, version, err_msg;
        size_t size_bytes = 0;
        if (!validateQAppHeader(data, len, app_name, version, size_bytes, &err_msg)) {
            server.send(400, "application/json", "{\"error\":\"" + err_msg + "\"}");
            return;
        }

        String filename = server.hasArg("filename") ? server.arg("filename") : "";
        if (filename.length() == 0) {
            filename = app_name;
            filename.toLowerCase();
            filename.replace(" ", "_");
            filename += ".qapp";
        }
        if (!FileManager::isPathSafe("/apps/" + filename)) {
            server.send(400, "application/json", "{\"error\":\"invalid_app_filename\"}");
            return;
        }

        if (fileManager.write("/apps/" + filename, data, len)) {
            String resp = "{\"status\":\"success\",\"app_name\":\"" + app_name + "\",\"version\":\"" + version + "\",\"size_bytes\":" + String((unsigned long)size_bytes) + "}";
            server.send(200, "application/json", resp);
        } else {
            server.send(500, "application/json", "{\"error\":\"write_failed\"}");
        }
    });

    // 13. Mochi Pet Action API
    server.on("/api/v1/mochi/action", HTTP_POST, [&server, this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
                String action = doc["action"] | "";
                int helmet = doc["helmet"] | -1;
                if (handleMochiAction(action, helmet)) {
                    server.send(200, "application/json", "{\"status\":\"ok\"}");
                    return;
                }
            }
        }
        server.send(400, "application/json", "{\"error\":\"bad_request\"}");
    });

    // 14. Power Profile & Governor API
    server.on("/api/v1/power/profile", HTTP_GET, [&server, this]() {
        server.send(200, "application/json", generatePowerProfileJson());
    });
    server.on("/api/v1/power/profile", HTTP_POST, [&server, this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
                String profile = doc["profile"] | "BALANCED";
                bool ulp = doc["ulp"] | true;
                bool eco_led = doc["eco_led"] | false;
                bool eco_radio = doc["eco_radio"] | false;
                if (setPowerProfileState(profile, ulp, eco_led, eco_radio)) {
                    server.send(200, "application/json", "{\"status\":\"ok\"}");
                    return;
                }
            }
        }
        server.send(400, "application/json", "{\"error\":\"bad_profile\"}");
    });

    // 15. SIGINT Recon Scan API
    server.on("/api/v1/sigint/scan", HTTP_GET, [&server, this]() {
        server.send(200, "application/json", generateSigintScanJson());
    });
}
#endif

bool QLinkEngine::handleMochiAction(const String& action, int helmet_id) {
    if (action == "pet") {
#ifdef ARDUINO
        mochiPet.pet();
#endif
        return true;
    } else if (action == "feed") {
#ifdef ARDUINO
        mochiPet.feed();
#endif
        return true;
    } else if (action == "wake") {
#ifdef ARDUINO
        mochiPet.wakeUp();
#endif
        return true;
    } else if (action == "helmet") {
        if (helmet_id >= 0 && helmet_id < 5) {
#ifdef ARDUINO
            mochiPet.setHelmet((MochiHelmet)helmet_id);
#endif
            return true;
        }
        return false;
    }
    return false;
}

bool QLinkEngine::setPowerProfileState(const String& profile_str, bool ulp, bool eco_led, bool eco_radio) {
    int p = -1;
    if (profile_str == "PERFORMANCE") p = 0;
    else if (profile_str == "BALANCED") p = 1;
    else if (profile_str == "ENDURANCE") p = 2;
    else if (profile_str == "CUSTOM") p = 3;

    if (p < 0) return false;
#ifdef ARDUINO
    powerManager.setProfile((PowerProfile)p);
    powerManager.setUlpEnabled(ulp);
    powerManager.setEcoLedBlockEnabled(eco_led);
    powerManager.setEcoRadioCutEnabled(eco_radio);
#endif
    return true;
}

String QLinkEngine::generatePowerProfileJson() {
    String json;
    json.reserve(256);
#ifdef ARDUINO
    PowerProfile prof = powerManager.getProfile();
    float v = battery.readVoltage();
    int pct = battery.readPercentage();
    float current_ma = powerManager.getEstimatedCurrentMa();
    float runtime_h = powerManager.calculateEstimatedRuntimeHours(v, pct);
    bool ulp = powerManager.isUlpEnabled();
    bool eco_led = powerManager.isEcoLedBlockEnabled();
    bool eco_radio = powerManager.isEcoRadioCutEnabled();

    json = "{\"profile\":\"" + String(powerManager.getProfileNameCStr(prof)) + "\",";
    json += "\"cpu_mhz\":" + String(powerManager.getTargetCpuFreqMhz()) + ",";
    json += "\"battery_pct\":" + String(pct) + ",";
    json += "\"voltage_v\":" + String(v, 2) + ",";
    json += "\"current_ma\":" + String(current_ma, 1) + ",";
    json += "\"runtime_hours\":" + String(runtime_h, 1) + ",";
    json += "\"ulp_active\":" + String(ulp ? "true" : "false") + ",";
    json += "\"eco_led\":" + String(eco_led ? "true" : "false") + ",";
    json += "\"eco_radio\":" + String(eco_radio ? "true" : "false") + "}";
#else
    json = "{\"profile\":\"BALANCED\",\"cpu_mhz\":160,\"battery_pct\":84,\"voltage_v\":3.92,\"current_ma\":2.8,\"runtime_hours\":36.5,\"ulp_active\":true,\"eco_led\":false,\"eco_radio\":false}";
#endif
    return json;
}

String QLinkEngine::generateSigintScanJson() {
    String json;
    json.reserve(1024);
#ifdef ARDUINO
    const WifiChannelStat* ch_stats = wirelessRecon.getChannelStats();
    json = "{\"active_channel\":" + String(wirelessRecon.getActiveChannel()) + ",";
    json += "\"best_channel\":" + String(wirelessRecon.getBestChannel()) + ",";
    json += "\"packet_rate_pps\":" + String(wirelessRecon.getCurrentPacketRate()) + ",";
    json += "\"total_packets\":" + String(wirelessRecon.getTotalPackets()) + ",";
    json += "\"total_aps\":" + String(wirelessRecon.getTotalApsFound()) + ",";
    json += "\"deauth_count\":" + String(wirelessRecon.getDeauthCount()) + ",";
    json += "\"attack_detected\":" + String(wirelessRecon.isAttackDetected() ? "true" : "false") + ",";
    
    // Channel AP distribution
    json += "\"channels\":[";
    for (int i = 1; i <= 13; i++) {
        if (i > 1) json += ",";
        json += "{\"ch\":" + String(i) + ",\"aps\":" + String(ch_stats[i].ap_count) + ",\"rssi\":" + String(ch_stats[i].max_rssi) + "}";
    }
    json += "],";

    // BLE Targets
    int ble_count = wirelessRecon.getBleTargetCount();
    json += "\"targets\":[";
    for (int i = 0; i < ble_count && i < 8; i++) {
        const BleTarget* t = wirelessRecon.getBleTarget(i);
        if (t && t->active) {
            if (i > 0) json += ",";
            float d = wirelessRecon.getTargetEstimatedDistance(t->rssi);
            json += "{\"name\":\"" + String(t->name) + "\",\"mac\":\"" + String(t->mac) + "\",\"rssi\":" + String(t->rssi) + ",\"dist_m\":" + String(d, 2) + "}";
        }
    }
    json += "]}";
#else
    json = "{\"active_channel\":6,\"best_channel\":11,\"packet_rate_pps\":142,\"total_packets\":8920,\"total_aps\":14,\"deauth_count\":0,\"attack_detected\":false,\"channels\":[{\"ch\":1,\"aps\":3,\"rssi\":-72},{\"ch\":6,\"aps\":6,\"rssi\":-58},{\"ch\":11,\"aps\":1,\"rssi\":-80}],\"targets\":[{\"name\":\"TARGET_ALPHA\",\"mac\":\"DC:54:75:A1:02:11\",\"rssi\":-48,\"dist_m\":1.8}]}";
#endif
    return json;
}
