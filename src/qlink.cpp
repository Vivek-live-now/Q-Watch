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
#include "wifi_portal.h"
#include "vibration_manager.h"
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
        qlink.cancelRawIr();
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
        if (!pChar) return;
        size_t len = pChar->getLength();
        const uint8_t* pData = pChar->getData();
        if (pData && len > 0) {
            qlink.handleBleFilePacket(pData, len);
        } else {
            std::string val = pChar->getValue();
            if (!val.empty()) {
                qlink.handleBleFilePacket((const uint8_t*)val.data(), val.size());
            }
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
    upload_expected_seq(0),
    upload_last_chunk_time(0),
    upload_use_ram(false),
    upload_ram_buf(nullptr),
#ifndef ARDUINO
    upload_file(nullptr),
#endif
    raw_ir_in_progress(false),
    raw_ir_frequency(38000),
    raw_ir_duty_cycle(0.33f),
    raw_ir_expected_count(0),
    raw_ir_received_count(0),
    raw_ir_expected_seq(0),
    raw_ir_last_activity(0)
#ifndef ARDUINO
    ,
    last_tx_proto(""),
    last_tx_addr(0),
    last_tx_cmd(0),
    last_tx_nbits(0),
    last_tx_raw_count(0),
    last_tx_raw_freq(0)
#endif
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
    upload_expected_seq = 0;
    raw_ir_in_progress = false;
    raw_ir_expected_count = 0;
    raw_ir_received_count = 0;
    raw_ir_expected_seq = 0;

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
        BLEDevice::setMTU(517);
        s_ble_server = nullptr;
        s_char_command = nullptr;
        s_char_telemetry = nullptr;
        s_char_file = nullptr;
    } else {
        BLEDevice::setMTU(517);
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

    // Dynamically update Wi-Fi coexistence modem sleep and TX power
    if (wifiPortal.getState() != WifiState::OFF) {
        wifiPortal.configurePowerSave();
        wifiPortal.applyTxPower();
    }
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

    if (wifiPortal.getState() != WifiState::OFF) {
        wifiPortal.configurePowerSave();
        wifiPortal.applyTxPower();
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

    // 3. Raw IR session timeout guard (5s inactivity)
    if (raw_ir_in_progress && (now - raw_ir_last_activity > 5000)) {
        cancelRawIr();
        if (s_char_file) {
            String resp = "{\"status\":\"ERROR\",\"code\":408,\"error\":\"timeout\"}";
            s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
            s_char_file->notify();
        }
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
            uint16_t nbits = doc["nbits"] | 0;
            transmitParsedIr(proto, addr, cmd, nbits);
        }
    }
#endif
}

void QLinkEngine::handleBleFilePacket(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;

    // Binary file chunk packet: magic 0xFE, 0x01
    if (len >= 6 && data[0] == 0xFE && data[1] == 0x01) {
        if (!upload_in_progress) {
#ifdef ARDUINO
            if (s_char_file) {
                String resp = "{\"status\":\"ERROR\",\"code\":400,\"error\":\"not_started\"}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
#endif
            return;
        }
        uint16_t seq = ((uint16_t)data[2] << 8) | data[3];
        size_t chunk_len = ((size_t)data[4] << 8) | data[5];
        if (chunk_len == 0 || chunk_len > len - 6) {
            cancelFileUpload();
#ifdef ARDUINO
            if (s_char_file) {
                String resp = "{\"status\":\"ERROR\",\"code\":400,\"error\":\"invalid_chunk_length\"}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
#endif
            return;
        }
        if (seq != upload_expected_seq) {
            cancelFileUpload();
#ifdef ARDUINO
            if (s_char_file) {
                String resp = "{\"status\":\"ERROR\",\"code\":400,\"error\":\"sequence_mismatch\",\"expected\":" + String((unsigned long)upload_expected_seq) + ",\"received\":" + String((unsigned long)seq) + "}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
#endif
            return;
        }
        if (upload_expected_size > 0 && upload_received_size + chunk_len > upload_expected_size) {
            cancelFileUpload();
#ifdef ARDUINO
            if (s_char_file) {
                String resp = "{\"status\":\"ERROR\",\"code\":400,\"error\":\"oversized_file\"}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
#endif
            return;
        }
        bool ok = processFileChunk(data + 6, chunk_len);
        if (!ok) {
            cancelFileUpload();
#ifdef ARDUINO
            if (s_char_file) {
                String resp = "{\"status\":\"ERROR\",\"code\":500,\"error\":\"write_failed\"}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
#endif
            return;
        }
        return;
    }

    // Binary raw IR chunk packet: magic 0xFE, 0x03
    if (len >= 6 && data[0] == 0xFE && data[1] == 0x03) {
        uint16_t seq = ((uint16_t)data[2] << 8) | data[3];
        size_t chunk_len = ((size_t)data[4] << 8) | data[5];
        if (chunk_len <= len - 6) {
            bool ok = processRawIrChunk(seq, data + 6, chunk_len);
            if (!ok) {
#ifdef ARDUINO
                if (s_char_file) {
                    String resp = "{\"status\":\"ERROR\",\"code\":400,\"error\":\"raw_chunk_error\"}";
                    s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                    s_char_file->notify();
                }
#endif
                return;
            }
            if (raw_ir_expected_count > 0 && raw_ir_received_count >= raw_ir_expected_count) {
                bool txOk = finishRawIr(raw_ir_expected_count);
#ifdef ARDUINO
                if (s_char_file) {
                    String resp = txOk
                        ? ("{\"status\":\"OK\",\"code\":200,\"action\":\"IR_RAW_TRANSMIT\",\"count\":" + String((unsigned long)raw_ir_received_count) + "}")
                        : "{\"status\":\"ERROR\",\"code\":500,\"error\":\"transmit_failed\"}";
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
        if (upload_expected_size > 0 && upload_received_size + len > upload_expected_size) {
            cancelFileUpload();
            return;
        }
        bool ok = processFileChunk(data, len);
        if (!ok) {
            cancelFileUpload();
            return;
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
            bool ok = upload_in_progress && finishFileUpload(size);
            if (s_char_file) {
                String resp = ok ? ("{\"status\":\"OK\",\"code\":200,\"size\":" + String((unsigned long)size) + "}")
                                 : "{\"status\":\"ERROR\",\"code\":400,\"error\":\"incomplete_upload\"}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "LIST") == 0) {
            String path = doc["path"] | "/";
            FileInfo files[32];
            size_t count = fileManager.listDir(path, files, 32);
            String json = "{\"cmd\":\"LIST_RESP\",\"path\":\"" + path + "\",\"files\":[";
            for (size_t i = 0; i < count; i++) {
                String entry = (i > 0 ? "," : "") + String("{\"name\":\"") + files[i].name + "\",\"size\":" + String((unsigned long)files[i].size) + ",\"is_dir\":" + (files[i].isDir ? "true" : "false") + "}";
                if (json.length() + entry.length() + 2 > 500) {
                    break;
                }
                json += entry;
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
        } else if (strcmp(cmd, "MKDIR") == 0) {
            String path = doc["path"] | "";
            bool ok = false;
            if (FileManager::isPathSafe(path) && path.length() > 0) {
                ok = LittleFS.mkdir(path);
            }
            if (s_char_file) {
                String resp = ok ? ("{\"status\":\"OK\",\"created\":\"" + path + "\"}")
                                 : "{\"status\":\"ERROR\",\"code\":500}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "RENAME") == 0) {
            String from = doc["from"] | (doc["old"] | "");
            String to = doc["to"] | (doc["new"] | "");
            bool ok = false;
            if (FileManager::isPathSafe(from) && FileManager::isPathSafe(to) && LittleFS.exists(from)) {
                FileManager::ensureParentDir(to);
                ok = LittleFS.rename(from, to);
            }
            if (s_char_file) {
                String resp = ok ? ("{\"status\":\"OK\",\"from\":\"" + from + "\",\"to\":\"" + to + "\"}")
                                 : "{\"status\":\"ERROR\",\"code\":500}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "COPY") == 0) {
            String src = doc["src"] | (doc["from"] | "");
            String dst = doc["dst"] | (doc["to"] | "");
            bool ok = false;
            if (FileManager::isPathSafe(src) && FileManager::isPathSafe(dst) && LittleFS.exists(src)) {
                File s = LittleFS.open(src, FILE_READ);
                if (s && !s.isDirectory()) {
                    FileManager::ensureParentDir(dst);
                    File d = LittleFS.open(dst, FILE_WRITE);
                    if (d) {
                        uint8_t buf[256];
                        while (s.available()) {
                            size_t n = s.read(buf, sizeof(buf));
                            if (n > 0) d.write(buf, n);
                        }
                        d.close();
                        ok = true;
                    }
                    s.close();
                } else if (s) {
                    s.close();
                }
            }
            if (s_char_file) {
                String resp = ok ? ("{\"status\":\"OK\",\"copied\":\"" + src + "\",\"to\":\"" + dst + "\"}")
                                 : "{\"status\":\"ERROR\",\"code\":500}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "DOWNLOAD") == 0) {
            String path = doc["path"] | "";
            if (!FileManager::isPathSafe(path) || !LittleFS.exists(path)) {
                if (s_char_file) {
                    String resp = "{\"status\":\"ERROR\",\"code\":404,\"message\":\"File not found\"}";
                    s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                    s_char_file->notify();
                }
            } else {
                File f = LittleFS.open(path, FILE_READ);
                if (!f || f.isDirectory()) {
                    if (f) f.close();
                    if (s_char_file) {
                        String resp = "{\"status\":\"ERROR\",\"code\":400,\"message\":\"Cannot download directory\"}";
                        s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                        s_char_file->notify();
                    }
                } else {
                    size_t total_size = f.size();
                    if (s_char_file) {
                        String start_resp = "{\"cmd\":\"DOWNLOAD_START\",\"path\":\"" + path + "\",\"size\":" + String((unsigned long)total_size) + "}";
                        s_char_file->setValue((uint8_t*)start_resp.c_str(), start_resp.length());
                        s_char_file->notify();
                    }
                    delay(25);

                    uint8_t chunk_buf[256];
                    size_t max_payload = 180;
                    uint16_t seq = 0;
                    size_t sent = 0;

                    while (f.available() > 0) {
                        size_t to_read = f.available();
                        if (to_read > max_payload) to_read = max_payload;
                        chunk_buf[0] = 0xFE;
                        chunk_buf[1] = 0x02;
                        chunk_buf[2] = (uint8_t)((seq >> 8) & 0xFF);
                        chunk_buf[3] = (uint8_t)(seq & 0xFF);
                        chunk_buf[4] = (uint8_t)((to_read >> 8) & 0xFF);
                        chunk_buf[5] = (uint8_t)(to_read & 0xFF);
                        size_t read_bytes = f.read(chunk_buf + 6, to_read);
                        if (read_bytes == 0) break;

                        if (s_char_file) {
                            s_char_file->setValue(chunk_buf, 6 + read_bytes);
                            s_char_file->notify();
                        }
                        sent += read_bytes;
                        seq++;
                        delay(20);
                    }
                    f.close();

                    delay(25);
                    if (s_char_file) {
                        String end_resp = "{\"cmd\":\"DOWNLOAD_END\",\"path\":\"" + path + "\",\"size\":" + String((unsigned long)sent) + "}";
                        s_char_file->setValue((uint8_t*)end_resp.c_str(), end_resp.length());
                        s_char_file->notify();
                    }
                }
            }
        } else if (strcmp(cmd, "STORAGE") == 0) {
            String json = generateStorageJson();
            if (s_char_file) {
                s_char_file->setValue((uint8_t*)json.c_str(), json.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "IR_RAW_START") == 0) {
            size_t count = doc["count"] | 0;
            uint32_t freq = doc["frequency"] | 38000;
            float duty = doc["duty_cycle"] | 0.33f;
            bool ok = startRawIr(count, freq, duty);
            if (s_char_file) {
                String resp = ok
                    ? ("{\"status\":\"OK\",\"code\":200,\"cmd\":\"IR_RAW_START\",\"count\":" + String((unsigned long)count) + "}")
                    : (count > 1024 ? "{\"status\":\"ERROR\",\"code\":413,\"error\":\"timings_overflow\"}"
                                    : "{\"status\":\"ERROR\",\"code\":400,\"error\":\"invalid_count\"}");
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "IR_RAW_FINISH") == 0) {
            size_t count = doc["count"] | raw_ir_expected_count;
            bool ok = finishRawIr(count);
            if (s_char_file) {
                String resp = ok
                    ? ("{\"status\":\"OK\",\"code\":200,\"action\":\"IR_RAW_TRANSMIT\",\"count\":" + String((unsigned long)count) + "}")
                    : "{\"status\":\"ERROR\",\"code\":500,\"error\":\"transmit_failed\"}";
                s_char_file->setValue((uint8_t*)resp.c_str(), resp.length());
                s_char_file->notify();
            }
        } else if (strcmp(cmd, "IR_TRANSMIT") == 0) {
            String proto = doc["protocol"] | "";
            uint32_t addr = doc["address"] | 0;
            uint32_t cmd_code = doc["command"] | 0;
            uint16_t nbits = doc["nbits"] | 0;
            bool ok = transmitParsedIr(proto, addr, cmd_code, nbits);
            if (s_char_file) {
                uint16_t actual_bits = (nbits > 0) ? nbits :
#ifdef ARDUINO
                    IREngine::getProtocolDefaultBits(proto);
#else
                    32;
#endif
                String resp = ok
                    ? ("{\"status\":\"OK\",\"code\":200,\"action\":\"IR_TRANSMIT\",\"protocol\":\"" + proto + "\",\"nbits\":" + String((unsigned int)actual_bits) + "}")
                    : "{\"status\":\"ERROR\",\"code\":400,\"error\":\"unsupported_protocol\"}";
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
    String clean_path = FileManager::normalizePath(path);
    if (!FileManager::isPathSafe(clean_path)) return false;

    upload_file_path = clean_path;
    upload_expected_size = total_size;
    upload_received_size = 0;
    upload_expected_seq = 0;
    upload_last_chunk_time = millis();
    upload_use_ram = false;
    upload_ram_buf = nullptr;

    // Buffer in RAM if total_size <= 128KB and sufficient free heap is available (> 35KB headroom).
    // Eliminates SPI flash write latency during BLE transfers to completely prevent packet dropping.
    if (total_size > 0 && total_size <= (128 * 1024)) {
        if (ESP.getFreeHeap() > (total_size + 35000)) {
            upload_ram_buf = (uint8_t*)malloc(total_size);
            if (upload_ram_buf != nullptr) {
                upload_use_ram = true;
            }
        }
    }

    if (!upload_use_ram) {
        FileManager::ensureParentDir(clean_path);
        if (animEngine.isOpen() && strcmp(animEngine.getFilePath(), clean_path.c_str()) == 0) {
            animEngine.stop();
            animEngine.close();
        }
        if (LittleFS.exists(clean_path)) {
            LittleFS.remove(clean_path);
        }
        upload_file = LittleFS.open(clean_path, FILE_WRITE);
        if (!upload_file) return false;
    }
#else
    String clean_path = path;
    upload_file_path = clean_path;
    upload_expected_size = total_size;
    upload_received_size = 0;
    upload_expected_seq = 0;
    upload_last_chunk_time = 0;
    upload_use_ram = false;
    upload_ram_buf = nullptr;
    if (upload_file) {
        fclose(upload_file);
        upload_file = nullptr;
    }
    upload_file = fopen(clean_path.c_str(), "wb");
#endif
    upload_in_progress = true;
    return true;
}

bool QLinkEngine::processFileChunk(const uint8_t* chunk, size_t chunk_len) {
    if (!upload_in_progress || !chunk || chunk_len == 0) return false;
#ifdef ARDUINO
    if (upload_use_ram) {
        if (!upload_ram_buf) return false;
        if (upload_expected_size > 0 && (upload_received_size + chunk_len > upload_expected_size)) {
            return false;
        }
        memcpy(upload_ram_buf + upload_received_size, chunk, chunk_len);
    } else {
        if (!upload_file) return false;
        size_t written = upload_file.write(chunk, chunk_len);
        if (written != chunk_len) return false;
    }
    upload_last_chunk_time = millis();
#else
    if (upload_file) {
        size_t written = fwrite(chunk, 1, chunk_len, upload_file);
        if (written != chunk_len) return false;
    }
#endif
    upload_received_size += chunk_len;
    upload_expected_seq++;
    return true;
}

bool QLinkEngine::finishFileUpload(size_t expected_size) {
    if (!upload_in_progress) return false;
    if (expected_size > 0 && upload_received_size != expected_size) {
        cancelFileUpload();
        return false;
    }
    if (upload_expected_size > 0 && upload_received_size != upload_expected_size) {
        cancelFileUpload();
        return false;
    }
#ifdef ARDUINO
    // Guard against corrupt/truncated uploads
    if (upload_expected_size > 0 && upload_received_size != upload_expected_size) {
        cancelFileUpload();
        return false;
    }

    if (upload_use_ram) {
        if (!upload_ram_buf || upload_received_size == 0) {
            cancelFileUpload();
            return false;
        }

        FileManager::ensureParentDir(upload_file_path);
        if (animEngine.isOpen() && strcmp(animEngine.getFilePath(), upload_file_path.c_str()) == 0) {
            animEngine.stop();
            animEngine.close();
        }
        if (LittleFS.exists(upload_file_path)) {
            LittleFS.remove(upload_file_path);
        }

        File f = LittleFS.open(upload_file_path, FILE_WRITE);
        if (!f) {
            cancelFileUpload();
            return false;
        }

        size_t written = f.write(upload_ram_buf, upload_received_size);
        f.flush();
        f.close();

        free(upload_ram_buf);
        upload_ram_buf = nullptr;
        upload_use_ram = false;

        if (written != upload_received_size) {
            LittleFS.remove(upload_file_path);
            upload_in_progress = false;
            return false;
        }
    } else {
        if (upload_file) {
            upload_file.flush();
            upload_file.close();
        }
    }

    String lower_path = upload_file_path;
    lower_path.toLowerCase();
    bool valid = true;
    if (lower_path.endsWith(".anim")) {
        File check = LittleFS.open(upload_file_path, "r");
        valid = false;
        if (check && check.size() >= 16) {
            uint8_t hdr[16];
            if (check.read(hdr, 16) == 16) {
                uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
                uint8_t w = hdr[6];
                uint8_t h = hdr[7];
                uint16_t frames = (uint16_t)hdr[8] | ((uint16_t)hdr[9] << 8);
                size_t exp_len = 16 + (size_t)frames * 1024;
                if (magic == 0x4D4E4151 && w == 128 && h == 64 && frames > 0 && check.size() == exp_len) {
                    valid = true;
                }
            }
        }
        if (check) check.close();
    } else if (lower_path.endsWith(".bmp")) {
        File check = LittleFS.open(upload_file_path, "r");
        valid = false;
        if (check && check.size() >= 54) {
            uint8_t hdr[54];
            if (check.read(hdr, 54) == 54) {
                if (hdr[0] == 'B' && hdr[1] == 'M') {
                    uint32_t offset = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8) | ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
                    int32_t w = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8) | ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
                    int32_t h = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8) | ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
                    uint16_t planes = (uint16_t)hdr[26] | ((uint16_t)hdr[27] << 8);
                    uint16_t bpp = (uint16_t)hdr[28] | ((uint16_t)hdr[29] << 8);
                    uint32_t comp = (uint32_t)hdr[30] | ((uint32_t)hdr[31] << 8) | ((uint32_t)hdr[32] << 16) | ((uint32_t)hdr[33] << 24);
                    if (offset >= 54 && w > 0 && h != 0 && planes == 1 && (comp == 0 || comp == 3) && (bpp == 1 || bpp == 8 || bpp == 24 || bpp == 32)) {
                        valid = true;
                    }
                }
            }
        }
        if (check) check.close();
    }
    if (!valid) {
        LittleFS.remove(upload_file_path);
        upload_in_progress = false;
        upload_file_path = "";
        upload_expected_size = 0;
        upload_received_size = 0;
        upload_expected_seq = 0;
        return false;
    }
    notifyFileReceived(upload_file_path);
#else
    if (upload_file) {
        fflush(upload_file);
        fclose(upload_file);
        upload_file = nullptr;
    }
    String lower_path = upload_file_path;
    lower_path.toLowerCase();
    bool valid = true;
    if (lower_path.endsWith(".anim")) {
        FILE* check = fopen(upload_file_path.c_str(), "rb");
        valid = false;
        if (check) {
            fseek(check, 0, SEEK_END);
            long sz = ftell(check);
            fseek(check, 0, SEEK_SET);
            if (sz >= 16) {
                uint8_t hdr[16];
                if (fread(hdr, 1, 16, check) == 16) {
                    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
                    uint8_t w = hdr[6];
                    uint8_t h = hdr[7];
                    uint16_t frames = (uint16_t)hdr[8] | ((uint16_t)hdr[9] << 8);
                    size_t exp_len = 16 + (size_t)frames * 1024;
                    if (magic == 0x4D4E4151 && w == 128 && h == 64 && frames > 0 && (size_t)sz == exp_len) {
                        valid = true;
                    }
                }
            }
            fclose(check);
        }
    } else if (lower_path.endsWith(".bmp")) {
        FILE* check = fopen(upload_file_path.c_str(), "rb");
        valid = false;
        if (check) {
            fseek(check, 0, SEEK_END);
            long sz = ftell(check);
            fseek(check, 0, SEEK_SET);
            if (sz >= 54) {
                uint8_t hdr[54];
                if (fread(hdr, 1, 54, check) == 54) {
                    if (hdr[0] == 'B' && hdr[1] == 'M') {
                        uint32_t offset = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8) | ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
                        int32_t w = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8) | ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
                        int32_t h = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8) | ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
                        uint16_t planes = (uint16_t)hdr[26] | ((uint16_t)hdr[27] << 8);
                        uint16_t bpp = (uint16_t)hdr[28] | ((uint16_t)hdr[29] << 8);
                        uint32_t comp = (uint32_t)hdr[30] | ((uint32_t)hdr[31] << 8) | ((uint32_t)hdr[32] << 16) | ((uint32_t)hdr[33] << 24);
                        if (offset >= 54 && w > 0 && h != 0 && planes == 1 && (comp == 0 || comp == 3) && (bpp == 1 || bpp == 8 || bpp == 24 || bpp == 32)) {
                            valid = true;
                        }
                    }
                }
            }
            fclose(check);
        }
    }
    if (!valid) {
        remove(upload_file_path.c_str());
        upload_in_progress = false;
        upload_file_path = "";
        upload_expected_size = 0;
        upload_received_size = 0;
        upload_expected_seq = 0;
        return false;
    }
#endif
    upload_in_progress = false;
    return true;
}

void QLinkEngine::cancelFileUpload() {
    if (!upload_in_progress && !upload_ram_buf) return;
#ifdef ARDUINO
    if (upload_ram_buf) {
        free(upload_ram_buf);
        upload_ram_buf = nullptr;
    }
    upload_use_ram = false;

    if (upload_file) {
        upload_file.close();
    }
    if (upload_file_path.length() > 0 && LittleFS.exists(upload_file_path)) {
        LittleFS.remove(upload_file_path);
    }
#else
    if (upload_file) {
        fclose(upload_file);
        upload_file = nullptr;
        if (upload_file_path.length() > 0) {
            remove(upload_file_path.c_str());
        }
    }
#endif
    upload_in_progress = false;
    upload_file_path = "";
    upload_expected_size = 0;
    upload_received_size = 0;
    upload_expected_seq = 0;
}

bool QLinkEngine::transmitParsedIr(const String& proto, uint32_t addr, uint32_t cmd, uint16_t nbits) {
    if (proto.length() == 0) return false;
    uint16_t bits = nbits;
    if (bits == 0) {
#ifdef ARDUINO
        bits = IREngine::getProtocolDefaultBits(proto);
#else
        String p = proto;
        p.trim();
        if (p.equalsIgnoreCase("RCA") || p.equalsIgnoreCase("NIKAI")) bits = 24;
        else if (p.equalsIgnoreCase("SONY") || p.equalsIgnoreCase("SIRC") || p.equalsIgnoreCase("RC5")) bits = 12;
        else if (p.equalsIgnoreCase("SIRC15") || p.equalsIgnoreCase("SHARP") || p.equalsIgnoreCase("DENON")) bits = 15;
        else if (p.equalsIgnoreCase("SIRC20") || p.equalsIgnoreCase("RC6")) bits = 20;
        else if (p.equalsIgnoreCase("RC5X")) bits = 13;
        else if (p.equalsIgnoreCase("JVC")) bits = 16;
        else if (p.equalsIgnoreCase("PANASONIC")) bits = 48;
        else if (p.equalsIgnoreCase("NEC42")) bits = 42;
        else bits = 32;
#endif
    }
    if (bits == 0 || bits > 128) return false;
    String p = proto;
    p.trim();
    if ((p.equalsIgnoreCase("RCA") || p.equalsIgnoreCase("NIKAI")) && bits != 24) {
        return false;
    }
#ifdef ARDUINO
    return irEngine.sendParsed(proto, addr, cmd, bits);
#else
    last_tx_proto = proto;
    last_tx_addr = addr;
    last_tx_cmd = cmd;
    last_tx_nbits = bits;
    return true;
#endif
}

bool QLinkEngine::startRawIr(size_t count, uint32_t frequency, float duty_cycle) {
    if (count == 0 || count > 1024) return false;
    if (frequency > 100000) return false;
    raw_ir_in_progress = true;
    raw_ir_expected_count = count;
    raw_ir_received_count = 0;
    raw_ir_frequency = frequency > 0 ? frequency : 38000;
    raw_ir_duty_cycle = (duty_cycle > 0.05f && duty_cycle < 0.95f) ? duty_cycle : 0.33f;
    raw_ir_expected_seq = 0;
#ifdef ARDUINO
    raw_ir_last_activity = millis();
#else
    raw_ir_last_activity = 0;
#endif
    return true;
}

bool QLinkEngine::processRawIrChunk(uint16_t seq, const uint8_t* data, size_t len) {
    if (!raw_ir_in_progress || !data || len == 0 || (len % 2 != 0)) {
        return false;
    }
    if (seq != raw_ir_expected_seq) {
        cancelRawIr();
        return false;
    }
    size_t timings_in_chunk = len / 2;
    if (raw_ir_received_count + timings_in_chunk > raw_ir_expected_count) {
        cancelRawIr();
        return false;
    }
    for (size_t i = 0; i < len; i += 2) {
        uint16_t t = (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8);
        if (t == 0) {
            cancelRawIr();
            return false;
        }
        raw_ir_timings[raw_ir_received_count++] = t;
    }
    raw_ir_expected_seq++;
#ifdef ARDUINO
    raw_ir_last_activity = millis();
#endif
    return true;
}

bool QLinkEngine::finishRawIr(size_t count) {
    if (!raw_ir_in_progress) return false;
    if (count > 0 && count != raw_ir_expected_count) {
        cancelRawIr();
        return false;
    }
    if (raw_ir_received_count != raw_ir_expected_count) {
        cancelRawIr();
        return false;
    }
    bool success = false;
#ifdef ARDUINO
    success = irEngine.sendRaw(raw_ir_timings, raw_ir_received_count, raw_ir_frequency);
#else
    last_tx_raw_count = raw_ir_received_count;
    last_tx_raw_freq = raw_ir_frequency;
    success = true;
#endif
    raw_ir_in_progress = false;
    return success;
}

void QLinkEngine::cancelRawIr() {
    raw_ir_in_progress = false;
    raw_ir_received_count = 0;
    raw_ir_expected_count = 0;
    raw_ir_expected_seq = 0;
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

void QLinkEngine::notifyFileReceived(const String& path) {
#ifdef ARDUINO
    ui.wakeDisplay();

    String fname = path;
    int lastSlash = fname.lastIndexOf('/');
    if (lastSlash >= 0) fname = fname.substring(lastSlash + 1);
    String lower = fname;
    lower.toLowerCase();

    String toast;
    if (path.startsWith("/mochi") || path.indexOf("mochi") >= 0) {
        toast = "[MOCHI] " + fname;
    } else if (lower.endsWith(".anim")) {
        toast = "[ANIM] " + fname;
    } else if (lower.endsWith(".bmp")) {
        toast = "[PHOTO] " + fname;
    } else if (lower.endsWith(".ir")) {
        toast = "[IR] " + fname;
    } else {
        toast = "[FILE] " + fname;
    }

    ui.showToast(toast.c_str(), 3000);

    if (soundManager.isNotificationsEnabled() && soundManager.isMasterSwitchOn()) {
        soundManager.playNotification();
    } else {
        soundManager.playTone(2200, 150);
    }

    vibrationManager.triggerPattern(VibePattern::DOUBLE_PULSE);
    ledManager.triggerPulse(CRGB::Cyan, 2, 80);

    // Refresh Mochi Pet's animations immediately
    mochiPet.loadMochiAnims();

    // If currently in Animation List or File Manager, refresh UI list view
    if (ui.getState() == UIState::APP_ANIM_LIST) {
        ui.loadAnimList();
    } else if (ui.getState() == UIState::APP_FILE_MANAGER) {
        ui.loadDirectory(ui.getFmCurrentPath());
    }
#else
    (void)path;
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
static File s_wifi_upload_file;
static String s_wifi_upload_path;
static bool s_wifi_upload_success = false;
static String s_wifi_upload_err = "";

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
                uint16_t nbits = doc["nbits"] | 0;
                if (transmitParsedIr(proto, addr, cmd, nbits)) {
                    server.send(200, "application/json", "{\"status\":\"transmitted\"}");
                    return;
                }
            }
        }
        server.send(400, "application/json", "{\"error\":\"invalid_ir_packet\"}");
    });

    server.on("/api/v1/ir/raw_transmit", HTTP_POST, [&server, this]() {
        if (server.hasArg("plain")) {
            JsonDocument doc;
            if (deserializeJson(doc, server.arg("plain")) == DeserializationError::Ok) {
                uint32_t freq = doc["frequency"] | 38000;
                float duty = doc["duty_cycle"] | 0.33f;
                JsonArray arr = doc["timings"].as<JsonArray>();
                size_t count = arr.size();
                if (count > 0 && count <= 1024 && startRawIr(count, freq, duty)) {
                    bool valid = true;
                    for (JsonVariant v : arr) {
                        uint32_t t = v.as<uint32_t>();
                        if (t == 0 || t > 65535) { valid = false; break; }
                        raw_ir_timings[raw_ir_received_count++] = (uint16_t)t;
                    }
                    if (valid && finishRawIr(count)) {
                        server.send(200, "application/json", "{\"status\":\"transmitted\",\"count\":" + String((unsigned long)count) + "}");
                        return;
                    }
                    cancelRawIr();
                }
            }
        }
        server.send(400, "application/json", "{\"error\":\"invalid_raw_ir_packet\"}");
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
        String path = FileManager::normalizePath(server.arg("path"));
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

    // 11. Filesystem Delete (with safe active animation cleanup)
    auto handleDelete = [&server, this]() {
        if (!server.hasArg("path")) {
            server.send(400, "application/json", "{\"error\":\"missing_path\"}");
            return;
        }
        String path = FileManager::normalizePath(server.arg("path"));
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
            String path = FileManager::normalizePath(server.arg("path"));
            if (!FileManager::isPathSafe(path)) {
                server.send(400, "application/json", "{\"error\":\"invalid_path\"}");
                return;
            }
            if (animEngine.isOpen()) {
                animEngine.stop();
                animEngine.close();
            }
            const String& body = server.arg("plain");
            String lower = path;
            lower.toLowerCase();

            // Animation validation using QANM header
            if (lower.endsWith(".anim")) {
                uint16_t f = 0, d = 0;
                String err;
                if (!validateAnimHeader((const uint8_t*)body.c_str(), body.length(), f, d, &err)) {
                    server.send(400, "application/json", "{\"error\":\"invalid_qanm_header\",\"detail\":\"" + err + "\"}");
                    return;
                }
            } else if (lower.endsWith(".bmp")) {
                bool valid = false;
                if (body.length() >= 54) {
                    const uint8_t* hdr = (const uint8_t*)body.c_str();
                    if (hdr[0] == 'B' && hdr[1] == 'M') {
                        uint32_t offset = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8) | ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
                        int32_t w = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8) | ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
                        int32_t h = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8) | ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
                        uint16_t planes = (uint16_t)hdr[26] | ((uint16_t)hdr[27] << 8);
                        uint16_t bpp = (uint16_t)hdr[28] | ((uint16_t)hdr[29] << 8);
                        uint32_t comp = (uint32_t)hdr[30] | ((uint32_t)hdr[31] << 8) | ((uint32_t)hdr[32] << 16) | ((uint32_t)hdr[33] << 24);
                        if (offset >= 54 && w > 0 && h != 0 && planes == 1 && (comp == 0 || comp == 3) && (bpp == 1 || bpp == 8 || bpp == 24 || bpp == 32)) {
                            valid = true;
                        }
                    }
                }
                if (!valid) {
                    server.send(400, "application/json", "{\"error\":\"invalid_bmp_format\"}");
                    return;
                }
            }

            if (fileManager.write(path, (const uint8_t*)body.c_str(), body.length())) {
                notifyFileReceived(path);
                server.send(200, "application/json", "{\"status\":\"uploaded\",\"path\":\"" + path + "\",\"size\":" + String((unsigned long)body.length()) + "}");
                return;
            } else {
                server.send(500, "application/json", "{\"error\":\"write_failed\"}");
                return;
            }
        }
        // Multipart upload completion response
        HTTPUpload& upload = server.upload();
        if (s_wifi_upload_success && upload.status == UPLOAD_FILE_END) {
            server.send(200, "application/json", "{\"status\":\"uploaded\",\"size\":" + String((unsigned long)upload.totalSize) + ",\"path\":\"" + s_wifi_upload_path + "\"}");
        } else {
            String err = s_wifi_upload_err.length() > 0 ? s_wifi_upload_err : "upload_failed";
            server.send(400, "application/json", "{\"error\":\"" + err + "\"}");
        }
    }, [&server, this]() {
        // Multipart streaming chunk handler with abort recovery
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            s_wifi_upload_success = false;
            s_wifi_upload_err = "";
            if (animEngine.isOpen()) {
                animEngine.stop();
                animEngine.close();
            }
            s_wifi_upload_path = FileManager::normalizePath(server.hasArg("path") ? server.arg("path") : ("/" + upload.filename));
            if (!FileManager::isPathSafe(s_wifi_upload_path)) {
                s_wifi_upload_err = "invalid_path";
                return;
            }
            FileManager::ensureParentDir(s_wifi_upload_path);
            if (LittleFS.exists(s_wifi_upload_path)) {
                LittleFS.remove(s_wifi_upload_path);
            }
            s_wifi_upload_file = LittleFS.open(s_wifi_upload_path, FILE_WRITE);
            if (!s_wifi_upload_file) {
                s_wifi_upload_err = "open_failed";
                return;
            }
            s_wifi_upload_success = true;
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (s_wifi_upload_success && s_wifi_upload_file) {
                size_t written = s_wifi_upload_file.write(upload.buf, upload.currentSize);
                if (written != upload.currentSize) {
                    s_wifi_upload_success = false;
                    s_wifi_upload_err = "write_failed";
                    s_wifi_upload_file.close();
                    LittleFS.remove(s_wifi_upload_path);
                }
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (s_wifi_upload_file) {
                s_wifi_upload_file.flush();
                s_wifi_upload_file.close();
            }
            if (!s_wifi_upload_success) {
                LittleFS.remove(s_wifi_upload_path);
                return;
            }
            String lower = s_wifi_upload_path;
            lower.toLowerCase();
            // QANM validation on completed upload: purge if corrupted
            if (lower.endsWith(".anim")) {
                File check = LittleFS.open(s_wifi_upload_path, FILE_READ);
                bool valid = false;
                if (check && check.size() >= 16) {
                    uint8_t hdr[16];
                    if (check.read(hdr, 16) == 16) {
                        uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
                        uint8_t w = hdr[6];
                        uint8_t h = hdr[7];
                        uint16_t frames = (uint16_t)hdr[8] | ((uint16_t)hdr[9] << 8);
                        size_t exp_len = 16 + (size_t)frames * 1024;
                        if (magic == 0x4D4E4151 && w == 128 && h == 64 && frames > 0 && check.size() == exp_len) {
                            valid = true;
                        }
                    }
                }
                if (check) check.close();
                if (!valid) {
                    LittleFS.remove(s_wifi_upload_path); // Recover from bad upload
                    s_wifi_upload_success = false;
                    s_wifi_upload_err = "invalid_anim";
                } else {
                    notifyFileReceived(s_wifi_upload_path);
                }
            } else if (lower.endsWith(".bmp")) {
                File check = LittleFS.open(s_wifi_upload_path, FILE_READ);
                bool valid = false;
                if (check && check.size() >= 54) {
                    uint8_t hdr[54];
                    if (check.read(hdr, 54) == 54) {
                        if (hdr[0] == 'B' && hdr[1] == 'M') {
                            uint32_t offset = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8) | ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
                            int32_t w = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8) | ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
                            int32_t h = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8) | ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
                            uint16_t planes = (uint16_t)hdr[26] | ((uint16_t)hdr[27] << 8);
                            uint16_t bpp = (uint16_t)hdr[28] | ((uint16_t)hdr[29] << 8);
                            uint32_t comp = (uint32_t)hdr[30] | ((uint32_t)hdr[31] << 8) | ((uint32_t)hdr[32] << 16) | ((uint32_t)hdr[33] << 24);
                            if (offset >= 54 && w > 0 && h != 0 && planes == 1 && (comp == 0 || comp == 3) && (bpp == 1 || bpp == 8 || bpp == 24 || bpp == 32)) {
                                valid = true;
                            }
                        }
                    }
                }
                if (check) check.close();
                if (!valid) {
                    LittleFS.remove(s_wifi_upload_path);
                    s_wifi_upload_success = false;
                    s_wifi_upload_err = "invalid_bmp";
                } else {
                    notifyFileReceived(s_wifi_upload_path);
                }
            } else {
                notifyFileReceived(s_wifi_upload_path);
            }
        } else if (upload.status == UPLOAD_FILE_ABORTED) {
            // Failed transfer recovery: immediately delete partial truncated file
            if (s_wifi_upload_file) {
                s_wifi_upload_file.close();
            }
            LittleFS.remove(s_wifi_upload_path);
            s_wifi_upload_success = false;
            s_wifi_upload_err = "upload_aborted";
        }
    });

    // 11c. Filesystem Storage Telemetry
    server.on("/api/v1/fs/storage", HTTP_GET, [&server, this]() {
        server.send(200, "application/json", generateStorageJson());
    });

    // 11d. Filesystem Make Directory
    auto handleFsMkdir = [&server]() {
        String path = server.hasArg("path") ? server.arg("path") : "";
        if (!FileManager::isPathSafe(path) || path.length() == 0) {
            server.send(400, "application/json", "{\"error\":\"invalid_path\"}");
            return;
        }
        if (LittleFS.mkdir(path)) {
            server.send(200, "application/json", "{\"status\":\"created\",\"path\":\"" + path + "\"}");
        } else {
            server.send(500, "application/json", "{\"error\":\"mkdir_failed\"}");
        }
    };
    server.on("/api/v1/fs/mkdir", HTTP_POST, handleFsMkdir);
    server.on("/api/v1/fs/mkdir", HTTP_GET, handleFsMkdir);

    // 11e. Filesystem Rename
    auto handleFsRename = [&server]() {
        String from = server.hasArg("from") ? server.arg("from") : (server.hasArg("old") ? server.arg("old") : "");
        String to = server.hasArg("to") ? server.arg("to") : (server.hasArg("new") ? server.arg("new") : "");
        if (!FileManager::isPathSafe(from) || !FileManager::isPathSafe(to) || !LittleFS.exists(from)) {
            server.send(400, "application/json", "{\"error\":\"invalid_path_or_source_missing\"}");
            return;
        }
        FileManager::ensureParentDir(to);
        if (LittleFS.rename(from, to)) {
            server.send(200, "application/json", "{\"status\":\"renamed\",\"from\":\"" + from + "\",\"to\":\"" + to + "\"}");
        } else {
            server.send(500, "application/json", "{\"error\":\"rename_failed\"}");
        }
    };
    server.on("/api/v1/fs/rename", HTTP_POST, handleFsRename);

    // 11f. Filesystem Copy
    auto handleFsCopy = [&server]() {
        String src = server.hasArg("src") ? server.arg("src") : (server.hasArg("from") ? server.arg("from") : "");
        String dst = server.hasArg("dst") ? server.arg("dst") : (server.hasArg("to") ? server.arg("to") : "");
        if (!FileManager::isPathSafe(src) || !FileManager::isPathSafe(dst) || !LittleFS.exists(src)) {
            server.send(400, "application/json", "{\"error\":\"invalid_path_or_source_missing\"}");
            return;
        }
        File s = LittleFS.open(src, FILE_READ);
        if (!s || s.isDirectory()) {
            if (s) s.close();
            server.send(400, "application/json", "{\"error\":\"source_is_directory_or_unreadable\"}");
            return;
        }
        FileManager::ensureParentDir(dst);
        File d = LittleFS.open(dst, FILE_WRITE);
        if (!d) {
            s.close();
            server.send(500, "application/json", "{\"error\":\"destination_open_failed\"}");
            return;
        }
        uint8_t buf[256];
        while (s.available()) {
            size_t n = s.read(buf, sizeof(buf));
            if (n > 0) d.write(buf, n);
        }
        s.close();
        d.close();
        server.send(200, "application/json", "{\"status\":\"copied\",\"src\":\"" + src + "\",\"dst\":\"" + dst + "\"}");
    };
    server.on("/api/v1/fs/copy", HTTP_POST, handleFsCopy);

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
