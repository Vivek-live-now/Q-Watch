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
#include "ir_engine.h"
#include "settings_data.h"
#include "air_mouse.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_gap_ble_api.h>
#include <ArduinoJson.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#ifdef ARDUINO
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

static BLEServer* s_ble_server = nullptr;
static BLECharacteristic* s_char_command = nullptr;
static BLECharacteristic* s_char_telemetry = nullptr;
static BLECharacteristic* s_char_file = nullptr;

class QLinkBleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        qlink.setBleConnected(true);
    }
    void onDisconnect(BLEServer* pServer) override {
        qlink.setBleConnected(false);
        qlink.cancelFileUpload();
        if (qlink.isBleEnabled()) {
            BLEAdvertising* pAdv = pServer->getAdvertising();
            if (pAdv) {
                pAdv->start();
            }
        }
    }
};

class QLinkBleCommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        std::string val = pChar->getValue();
        if (!val.empty()) {
            qlink.handleBleCommand((const uint8_t*)val.data(), val.size());
        }
    }
};

class QLinkBleFileCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* pChar) override {
        std::string val = pChar->getValue();
        if (!val.empty()) {
            qlink.handleBleFilePacket((const uint8_t*)val.data(), val.size());
        }
    }
};
#endif

QLinkEngine qlink;

QLinkEngine::QLinkEngine() :
    packet_seq(0),
    display_streaming(false),
    target_stream_fps(20),
    last_stream_frame_time(0),
    ble_active(false),
    ble_connected(false),
    ble_advertising_paused(false),
    last_telemetry_tx(0),
    upload_in_progress(false),
    upload_file_path(""),
    upload_expected_size(0),
    upload_received_size(0),
    upload_last_chunk_time(0)
{}

void QLinkEngine::begin() {
    packet_seq = 0;
    display_streaming = false;
    ble_connected = false;
    ble_active = false;
    ble_advertising_paused = false;
    upload_in_progress = false;
    upload_file_path = "";
    upload_expected_size = 0;
    upload_received_size = 0;

#ifdef ARDUINO
    // Only initialize and activate BLE if user has enabled it in settings
    if (settingsManager.get().ble_enabled) {
        startBle();
    }
#endif
}

void QLinkEngine::startBle() {
#ifdef ARDUINO
    if (ble_active && s_ble_server) {
        if (ble_advertising_paused) {
            resumeBleAdvertising();
        }
        return;
    }

    if (!BLEDevice::getInitialized()) {
        BLEDevice::init("Q-Watch");
        s_ble_server = nullptr;
        s_char_command = nullptr;
        s_char_telemetry = nullptr;
        s_char_file = nullptr;
    }

    if (!s_ble_server) {
        s_ble_server = BLEDevice::createServer();
        s_ble_server->setCallbacks(new QLinkBleServerCallbacks());

        BLEService* pService = s_ble_server->createService(QLINK_SERVICE_UUID);

        s_char_command = pService->createCharacteristic(
            QLINK_CHAR_COMMAND,
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
        );
        s_char_command->setCallbacks(new QLinkBleCommandCallbacks());

        s_char_telemetry = pService->createCharacteristic(
            QLINK_CHAR_TELEMETRY,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        s_char_telemetry->addDescriptor(new BLE2902());

        s_char_file = pService->createCharacteristic(
            QLINK_CHAR_FILE,
            BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR |
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
        );
        s_char_file->addDescriptor(new BLE2902());
        s_char_file->setCallbacks(new QLinkBleFileCallbacks());

        // Setup security callbacks so pairing does not freeze, timeout or corrupt NVS
        class QLinkSecurityCallbacks : public BLESecurityCallbacks {
            uint32_t onPassKeyRequest() override { return 0; }
            void onPassKeyNotify(uint32_t pass_key) override {}
            bool onConfirmPIN(uint32_t pass_key) override { return true; }
            bool onSecurityRequest() override { return true; }
            void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {}
        };
        BLEDevice::setSecurityCallbacks(new QLinkSecurityCallbacks());

        BLESecurity* pSecurity = new BLESecurity();
        pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
        pSecurity->setCapability(ESP_IO_CAP_NONE);
        pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

        pService->start();
    }

    // Set BLE RF TX power level to -3 dBm to eliminate excessive heating on ESP32-S3 SuperMini
    // Official ESP32-S3 datasheet: default TX (+9dBm) draws 204mA, +20dBm draws 340mA peak.
    // -3dBm cuts peak TX current while retaining strong 5m line-of-sight range.
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N3);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_N3);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_N3);

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    if (pAdvertising) {
        pAdvertising->stop();
        pAdvertising->addServiceUUID(QLINK_SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        // Balanced advertising interval (320ms - 640ms) reduces RF duty cycle while remaining responsive
        pAdvertising->setMinInterval(0x200); // 320ms
        pAdvertising->setMaxInterval(0x400); // 640ms
        // Preferred connection interval (40ms - 80ms) reduces active connection events from 133/sec to 12-25/sec
        pAdvertising->setMinPreferred(0x20); // 40ms
        pAdvertising->setMaxPreferred(0x40); // 80ms
        pAdvertising->start();
    }

    ble_active = true;
    ble_advertising_paused = false;
#endif
}

void QLinkEngine::stopBle() {
#ifdef ARDUINO
    if (!ble_active) return;

    if (upload_in_progress) {
        cancelFileUpload();
    }
    ble_connected = false;

    BLEAdvertising* pAdv = BLEDevice::getAdvertising();
    if (pAdv) {
        pAdv->stop();
    }

    ble_active = false;
    ble_advertising_paused = false;

    // Fully deinitialize BLE controller if Air Mouse is not using it,
    // stopping radio PHY and background tasks to return chip to cool idle state
    if (!airMouse.isEnabled()) {
        BLEDevice::deinit(false);
        s_ble_server = nullptr;
        s_char_command = nullptr;
        s_char_telemetry = nullptr;
        s_char_file = nullptr;
    }
#endif
}

void QLinkEngine::setBleEnabled(bool enabled) {
    if (enabled) {
        startBle();
    } else {
        stopBle();
    }
}

void QLinkEngine::pauseBleAdvertising() {
#ifdef ARDUINO
    if (ble_active && !ble_connected && !ble_advertising_paused) {
        BLEAdvertising* pAdv = BLEDevice::getAdvertising();
        if (pAdv) {
            pAdv->stop();
        }
        ble_advertising_paused = true;
    }
#endif
}

void QLinkEngine::resumeBleAdvertising() {
#ifdef ARDUINO
    if (ble_active && !ble_connected && ble_advertising_paused) {
        BLEAdvertising* pAdv = BLEDevice::getAdvertising();
        if (pAdv) {
            pAdv->start();
        }
        ble_advertising_paused = false;
    }
#endif
}

void QLinkEngine::clearBondedDevices() {
#ifdef ARDUINO
    if (!BLEDevice::getInitialized()) return;
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num > 0) {
        esp_ble_bond_dev_t* dev_list = (esp_ble_bond_dev_t*)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
        if (dev_list) {
            if (esp_ble_get_bond_device_list(&dev_num, dev_list) == ESP_OK) {
                for (int i = 0; i < dev_num; i++) {
                    esp_ble_remove_bond_device(dev_list[i].bd_addr);
                }
            }
            free(dev_list);
        }
    }
#endif
}

void QLinkEngine::loop() {
#ifdef ARDUINO
    uint32_t now = millis();
    // 1. Emit compact telemetry at 1 Hz when connected over BLE
    if (ble_connected && s_char_telemetry) {
        if (now - last_telemetry_tx >= 1000) {
            last_telemetry_tx = now;
            QLinkCompactTelemetry tel;
            fillCompactTelemetry(tel);
            s_char_telemetry->setValue((uint8_t*)&tel, sizeof(tel));
            s_char_telemetry->notify();
        }
    }

    // 2. Upload chunk timeout guard (15s inactivity)
    if (upload_in_progress && (now - upload_last_chunk_time > 15000)) {
        cancelFileUpload();
    }
#endif
}

void QLinkEngine::handleBleCommand(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;
#ifdef ARDUINO
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err == DeserializationError::Ok) {
        const char* action = doc["action"] | "";
        if (strcmp(action, "BUTTON") == 0) {
            String btn = doc["button"] | "";
            String evt = doc["event"] | "SHORT_PRESS";
            injectButton(btn, evt);
        } else if (strcmp(action, "NOTIFY") == 0) {
            String app = doc["app_name"] | "Phone";
            String title = doc["title"] | "";
            String body = doc["body"] | "";
            String alert = doc["alert"] | doc["alert_style"] | "CHIME";
            String led = doc["led_color"] | "#00E5FF";
            handleNotification(app, title, body, alert, led);
        } else if (strcmp(action, "SYNC_TIME") == 0) {
            uint32_t epoch = doc["epoch"] | 0;
            int32_t tz = doc["tz"] | 0;
            if (epoch > 0) syncTime(epoch, tz);
        } else if (strcmp(action, "SYNC_WEATHER") == 0) {
            String city = doc["city"] | "";
            float temp = doc["temp"] | 25.0f;
            int hum = doc["hum"] | 50;
            int code = doc["code"] | 800;
            String desc = doc["desc"] | "Clear";
            float hi = doc["hi"] | temp;
            float lo = doc["lo"] | temp;
            syncWeather(city, temp, hum, code, desc, hi, lo);
        } else if (strcmp(action, "IR_TRANSMIT") == 0) {
            String proto = doc["protocol"] | "";
            uint32_t addr = doc["address"] | 0;
            uint32_t cmd = doc["command"] | 0;
            uint16_t nbits = doc["nbits"] | 32;
            if (proto.length() > 0) {
                irEngine.sendParsed(proto, addr, cmd, nbits);
            }
        }
    }
#endif
}

void QLinkEngine::handleBleFilePacket(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    // Binary chunk packet: magic 0xFE, 0x01
    if (len >= 6 && data[0] == 0xFE && data[1] == 0x01) {
        size_t chunk_len = ((size_t)data[4] << 8) | data[5];
        if (chunk_len <= len - 6) {
            processFileChunk(data + 6, chunk_len);
            if (upload_expected_size > 0 && upload_received_size >= upload_expected_size) {
                finishFileUpload(upload_expected_size);
#ifdef ARDUINO
                if (s_char_file) {
                    String resp = "{\"status\":\"OK\",\"code\":200,\"message\":\"Complete\",\"size\":" + String((unsigned long)upload_received_size) + "}";
                    s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                    s_char_file->notify();
                }
#endif
            }
            return;
        }
    }

    // Raw chunk when upload is active and payload not starting with JSON '{'
    if (upload_in_progress && data[0] != '{') {
        processFileChunk(data, len);
        if (upload_expected_size > 0 && upload_received_size >= upload_expected_size) {
            finishFileUpload(upload_expected_size);
#ifdef ARDUINO
            if (s_char_file) {
                String resp = "{\"status\":\"OK\",\"code\":200,\"message\":\"Complete\",\"size\":" + String((unsigned long)upload_received_size) + "}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
#endif
        }
        return;
    }

    // JSON file command
#ifdef ARDUINO
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err == DeserializationError::Ok) {
        const char* cmd = doc["cmd"] | "";
        if (strcmp(cmd, "START") == 0) {
            String path = doc["path"] | "";
            size_t size = doc["size"] | 0;
            bool ok = startFileUpload(path, size);
            if (s_char_file) {
                String resp = ok ? ("{\"status\":\"OK\",\"code\":200,\"path\":\"" + path + "\"}")
                                 : "{\"status\":\"ERROR\",\"code\":400}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "FINISH") == 0) {
            size_t size = doc["size"] | upload_received_size;
            bool ok = finishFileUpload(size);
            if (s_char_file) {
                String resp = ok ? ("{\"status\":\"OK\",\"code\":200,\"size\":" + String((unsigned long)size) + "}")
                                 : "{\"status\":\"ERROR\",\"code\":500}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "LIST") == 0) {
            String path = doc["path"] | "/";
            FileInfo files[32];
            size_t count = fileManager.listDir(path, files, 32);
            String json = "{\"cmd\":\"LIST_RESP\",\"path\":\"" + path + "\",\"files\":[";
            for (size_t i = 0; i < count; i++) {
                if (i > 0) json += ",";
                json += "{\"name\":\"" + files[i].name + "\",\"size\":" + String((unsigned long)files[i].size) + ",\"is_dir\":" + (files[i].isDir ? "true" : "false") + "}";
            }
            json += "]}";
            if (s_char_file) {
                s_char_file->setValue((uint8_t*)json.c_str(), json.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "DELETE") == 0) {
            String path = doc["path"] | "";
            bool ok = safeDeleteAnim(path);
            if (!ok && FileManager::isPathSafe(path)) {
                ok = fileManager.remove(path);
            }
            if (s_char_file) {
                String resp = ok ? ("{\"status\":\"OK\",\"deleted\":\"" + path + "\"}")
                                 : "{\"status\":\"ERROR\",\"code\":404}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
        }
    }
#endif
}

bool QLinkEngine::startFileUpload(const String& path, size_t total_size) {
    if (path.length() == 0 || path.indexOf("..") != -1) return false;
    cancelFileUpload();
#ifdef ARDUINO
    if (!FileManager::isPathSafe(path)) return false;
    FileManager::ensureParentDir(path);
    upload_file = LittleFS.open(path, FILE_WRITE);
    if (!upload_file) return false;
    upload_last_chunk_time = millis();
#else
    upload_last_chunk_time = 0;
#endif
    upload_in_progress = true;
    upload_file_path = path;
    upload_expected_size = total_size;
    upload_received_size = 0;
    return true;
}

bool QLinkEngine::processFileChunk(const uint8_t* chunk, size_t chunk_len) {
    if (!upload_in_progress || !chunk || chunk_len == 0) return false;
#ifdef ARDUINO
    if (!upload_file) return false;
    size_t written = upload_file.write(chunk, chunk_len);
    if (written != chunk_len) return false;
    upload_last_chunk_time = millis();
#endif
    upload_received_size += chunk_len;
    return true;
}

bool QLinkEngine::finishFileUpload(size_t expected_size) {
    if (!upload_in_progress) return false;
#ifdef ARDUINO
    if (upload_file) {
        upload_file.flush();
        upload_file.close();
    }
    if (upload_file_path.endsWith(".ir")) {
        ui.showToast("IR Remote Loaded!", 2000);
        soundManager.playTone(2000, 100);
    }
#endif
    upload_in_progress = false;
    return true;
}

void QLinkEngine::cancelFileUpload() {
    if (!upload_in_progress) return;
#ifdef ARDUINO
    if (upload_file) {
        upload_file.close();
        if (upload_file_path.length() > 0) {
            LittleFS.remove(upload_file_path);
        }
    }
#endif
    upload_in_progress = false;
    upload_file_path = "";
    upload_expected_size = 0;
    upload_received_size = 0;
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

bool QLinkEngine::validateAnimHeader(const uint8_t* data, size_t len, uint16_t& out_frames, uint16_t& out_delay_ms, String* out_error) {
    if (!data || len < 16) {
        if (out_error) *out_error = "Payload smaller than AnimHeader (16B)";
        return false;
    }
    // Check magic: 0x4D4E4151 ("QANM" in little-endian)
    uint32_t magic = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    if (magic != 0x4D4E4151) {
        if (out_error) *out_error = "Invalid ANIM magic (expected 0x4D4E4151)";
        return false;
    }
    uint8_t w = data[6];
    uint8_t h = data[7];
    if (w != 128 || h != 64) {
        if (out_error) *out_error = "Invalid dimensions (expected 128x64)";
        return false;
    }
    uint16_t frames = (uint16_t)data[8] | ((uint16_t)data[9] << 8);
    uint16_t delay = (uint16_t)data[10] | ((uint16_t)data[11] << 8);
    if (frames == 0) {
        if (out_error) *out_error = "Frame count cannot be zero";
        return false;
    }
    size_t expected_size = 16 + (size_t)frames * 1024;
    if (len != expected_size) {
        if (out_error) *out_error = "Size mismatch: expected " + String((unsigned long)expected_size) + ", got " + String((unsigned long)len);
        return false;
    }
    out_frames = frames;
    out_delay_ms = (delay > 0) ? delay : 50;
    return true;
}

bool QLinkEngine::safeDeleteAnim(const String& path) {
    if (path.length() == 0 || path.indexOf("..") != -1) return false;
#ifdef ARDUINO
    if (!FileManager::isPathSafe(path)) return false;
    // Safe deletion: Stop and close file handle if currently playing in animEngine or mochiPet
    if (animEngine.isOpen() && strcmp(animEngine.getFilePath(), path.c_str()) == 0) {
        mochiPet.stopAnim();
        animEngine.stop();
        animEngine.close();
    }
    return fileManager.remove(path);
#else
    return true;
#endif
}

String QLinkEngine::generateStorageJson() {
    String json;
    json.reserve(256);
#ifdef ARDUINO
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    size_t free_bytes = (total >= used) ? (total - used) : 0;
    float free_pct = (total > 0) ? ((float)free_bytes / (float)total * 100.0f) : 0.0f;

    size_t anim_count = 0;
    File root = LittleFS.open("/mochi");
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory() && String(file.name()).endsWith(".anim")) {
                anim_count++;
            }
            file = root.openNextFile();
        }
        root.close();
    }
    size_t free_slots = free_bytes / (20 * 1024);

    json = "{\"status\":\"ok\",";
    json += "\"fs_total_bytes\":" + String((unsigned long)total) + ",";
    json += "\"fs_used_bytes\":" + String((unsigned long)used) + ",";
    json += "\"fs_free_bytes\":" + String((unsigned long)free_bytes) + ",";
    json += "\"free_pct\":" + String(free_pct, 1) + ",";
    json += "\"anim_count\":" + String((unsigned long)anim_count) + ",";
    json += "\"free_anim_slots\":" + String((unsigned long)free_slots) + "}";
#else
    json = "{\"status\":\"ok\",\"fs_total_bytes\":917504,\"fs_used_bytes\":491520,\"fs_free_bytes\":425984,\"free_pct\":46.4,\"anim_count\":27,\"free_anim_slots\":20}";
#endif
    return json;
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

    // 5b. Live IR Signal Blasting
    server.on("/api/v1/ir/transmit", HTTP_POST, [&server, this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
                String proto = doc["protocol"] | "";
                uint32_t addr = doc["address"] | 0;
                uint32_t cmd = doc["command"] | 0;
                uint16_t nbits = doc["nbits"] | 32;
                if (proto.length() > 0) {
                    irEngine.sendParsed(proto, addr, cmd, nbits);
                    server.send(200, "application/json", "{\"status\":\"transmitted\"}");
                    return;
                }
            }
        }
        server.send(400, "application/json", "{\"error\":\"invalid_ir_packet\"}");
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
    // 11. Filesystem Delete (with safe active animation cleanup)
    auto handleDelete = [&server, this]() {
        if (!server.hasArg("path")) {
            server.send(400, "application/json", "{\"error\":\"missing_path\"}");
            return;
        }
        String path = server.arg("path");
        if (!FileManager::isPathSafe(path)) {
            server.send(400, "application/json", "{\"error\":\"invalid_path\"}");
            return;
        }
        if (safeDeleteAnim(path)) {
            server.send(200, "application/json", "{\"status\":\"deleted\"}");
        } else {
            server.send(404, "application/json", "{\"error\":\"delete_failed\"}");
        }
    };
    server.on("/api/v1/fs/delete", HTTP_DELETE, handleDelete);
    server.on("/api/v1/fs/delete", HTTP_POST, handleDelete);

    // 11b. Filesystem Upload (supports multipart form-data and raw binary POST with QANM validation & abort recovery)
    server.on("/api/v1/fs/upload", HTTP_POST, [&server, this]() {
        // Plain body upload: POST /api/v1/fs/upload?path=/mochi/xyz.anim with body
        if (server.hasArg("path") && server.hasArg("plain")) {
            String path = server.arg("path");
            if (!FileManager::isPathSafe(path)) {
                server.send(400, "application/json", "{\"error\":\"invalid_path\"}");
                return;
            }
            const String& body = server.arg("plain");

            // Animation validation using QANM header
            if (path.endsWith(".anim")) {
                uint16_t f = 0, d = 0;
                String err;
                if (!validateAnimHeader((const uint8_t*)body.c_str(), body.length(), f, d, &err)) {
                    server.send(400, "application/json", "{\"error\":\"invalid_qanm_header\",\"detail\":\"" + err + "\"}");
                    return;
                }
            }

            if (fileManager.write(path, (const uint8_t*)body.c_str(), body.length())) {
                server.send(200, "application/json", "{\"status\":\"uploaded\",\"path\":\"" + path + "\",\"size\":" + String((unsigned long)body.length()) + "}");
                return;
            } else {
                server.send(500, "application/json", "{\"error\":\"write_failed\"}");
                return;
            }
        }
        // Multipart upload completion response
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_END) {
            server.send(200, "application/json", "{\"status\":\"uploaded\",\"size\":" + String((unsigned long)upload.totalSize) + "}");
        } else {
            server.send(200, "application/json", "{\"status\":\"ok\"}");
        }
    }, [&server, this]() {
        // Multipart streaming chunk handler with abort recovery
        static File s_upload_file;
        static String s_upload_path;
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            s_upload_path = server.hasArg("path") ? server.arg("path") : ("/" + upload.filename);
            if (!FileManager::isPathSafe(s_upload_path)) {
                return;
            }
            FileManager::ensureParentDir(s_upload_path);
            s_upload_file = LittleFS.open(s_upload_path, FILE_WRITE);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (s_upload_file) {
                s_upload_file.write(upload.buf, upload.currentSize);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (s_upload_file) {
                s_upload_file.close();
            }
            // QANM validation on completed upload: purge if corrupted
            if (s_upload_path.endsWith(".anim")) {
                File check = LittleFS.open(s_upload_path, FILE_READ);
                bool valid = false;
                if (check && check.size() >= 16) {
                    uint8_t hdr[16];
                    check.read(hdr, 16);
                    uint16_t f = 0, d = 0;
                    valid = validateAnimHeader(hdr, 16, f, d) || (check.size() == 16 + (size_t)(((uint16_t)hdr[8]) | (((uint16_t)hdr[9]) << 8)) * 1024);
                    check.close();
                } else if (check) {
                    check.close();
                }
                if (!valid) {
                    LittleFS.remove(s_upload_path); // Recover from bad upload
                }
            }
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            // Failed transfer recovery: immediately delete partial truncated file
            if (s_upload_file) {
                s_upload_file.close();
            }
            LittleFS.remove(s_upload_path);
        }
    });

    // 11c. Filesystem Storage Telemetry
    server.on("/api/v1/fs/storage", HTTP_GET, [&server, this]() {
        server.send(200, "application/json", generateStorageJson());
    });

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
