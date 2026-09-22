#include "ir_engine.h"
#include "file_manager.h"
#include <LittleFS.h>

IREngine irEngine;

static const TvBGoneCode TVB_CODES[] = {
    { NEC, 0x20DF10EF, 32, 38000 },       // LG
    { SAMSUNG, 0xE0E040BF, 32, 38000 },   // Samsung
    { SONY, 0xa90, 12, 40000 },           // Sony
    { PANASONIC, 0x100BCBD, 48, 37000 },  // Panasonic
    { RC5, 0xC, 12, 36000 },              // RC5
    { RC6, 0x1000C, 20, 36000 },          // RC6
    { SHARP, 0x41A2, 15, 38000 },         // Sharp
    { JVC, 0xC561, 16, 38000 },           // JVC
    { NEC, 0x807618E7, 32, 38000 },       // Vizio
    { NEC, 0x61A0F00F, 32, 38000 },       // Insignia
    { NEC, 0x02FD48B7, 32, 38000 }        // Sanyo
};

static const int TVB_COUNT = sizeof(TVB_CODES) / sizeof(TVB_CODES[0]);

IREngine::IREngine() :
    ir_recv(IR_RX, 1024, 50, true),
    ir_send(IR_TX),
    rx_active(false),
    tvbg_running(false),
    tvbg_current_idx(0),
    tvbg_total_codes(TVB_COUNT),
    tvbg_last_tx_time(0),
    recent_count(0) {
}

void IREngine::begin() {
    ir_send.begin();
    LittleFS.mkdir("/ir");
    LittleFS.mkdir("/ir/custom");
    LittleFS.mkdir("/ir/universal");
    LittleFS.mkdir("/ir/favorites");
    LittleFS.mkdir("/ir/recent");
}

void IREngine::startReceiver() {
    if (!rx_active) {
        ir_recv.enableIRIn();
        rx_active = true;
    }
}

void IREngine::stopReceiver() {
    if (rx_active) {
        ir_recv.disableIRIn();
        rx_active = false;
    }
}

void IREngine::sendSignal(const IrButton& btn) {
    bool was_rx = rx_active;
    if (was_rx) stopReceiver();

    if (btn.type == IrSignalType::PARSED) {
        ir_send.send(btn.protocol, btn.command, btn.bits, 1);
    } else {
        uint16_t freq = (btn.frequency > 0) ? btn.frequency : 38000;
        ir_send.sendRaw(btn.raw_data, btn.raw_len, freq / 1000);
    }

    if (was_rx) startReceiver();
}

bool IREngine::captureSignal(IrButton& btn, uint32_t timeout_ms) {
    startReceiver();
    decode_results results;
    uint32_t start_time = millis();

    while (millis() - start_time < timeout_ms) {
        if (ir_recv.decode(&results)) {
            if (results.overflow) {
                ir_recv.resume();
                continue;
            }

            btn.protocol = results.decode_type;
            btn.protocol_name = typeToString(results.decode_type);
            btn.bits = results.bits;

            if (results.decode_type != UNKNOWN) {
                btn.type = IrSignalType::PARSED;
                btn.command = results.value;
                btn.address = results.address;
                btn.frequency = 38000;
                btn.duty_cycle = 0.33f;
            } else {
                btn.type = IrSignalType::RAW;
                btn.frequency = 38000;
                btn.duty_cycle = 0.33f;
                btn.raw_len = results.rawlen - 1;
                for (uint16_t i = 1; i < results.rawlen && i - 1 < MAX_RAW_TIMINGS; i++) {
                    btn.raw_data[i - 1] = results.rawbuf[i] * kRawTick;
                }
            }

            ir_recv.resume();
            return true;
        }
        delay(10);
    }
    return false;
}

bool IREngine::checkLiveSignal(IrButton& btn) {
    startReceiver();
    decode_results results;
    if (ir_recv.decode(&results)) {
        if (!results.overflow && results.rawlen > 4) {
            btn.protocol = results.decode_type;
            btn.protocol_name = typeToString(results.decode_type);
            btn.bits = results.bits;

            if (results.decode_type != UNKNOWN) {
                btn.type = IrSignalType::PARSED;
                btn.command = results.value;
                btn.address = results.address;
                btn.frequency = 38000;
            } else {
                btn.type = IrSignalType::RAW;
                btn.frequency = 38000;
                btn.raw_len = results.rawlen - 1;
                for (uint16_t i = 1; i < results.rawlen && i - 1 < MAX_RAW_TIMINGS; i++) {
                    btn.raw_data[i - 1] = results.rawbuf[i] * kRawTick;
                }
            }
            ir_recv.resume();
            return true;
        }
        ir_recv.resume();
    }
    return false;
}

bool IREngine::reanalyzeRawToParsed(IrButton& btn) {
    if (btn.type == IrSignalType::PARSED || btn.raw_len == 0) return false;

    uint16_t raw_ticks[MAX_RAW_TIMINGS];
    for (uint16_t i = 0; i < btn.raw_len; i++) {
        raw_ticks[i + 1] = btn.raw_data[i] / kRawTick;
    }

    decode_results results;
    results.rawbuf = raw_ticks;
    results.rawlen = btn.raw_len + 1;

    if (ir_recv.decode(&results) && results.decode_type != UNKNOWN) {
        btn.type = IrSignalType::PARSED;
        btn.protocol = results.decode_type;
        btn.protocol_name = typeToString(results.decode_type);
        btn.command = results.value;
        btn.address = results.address;
        btn.bits = results.bits;
        return true;
    }
    return false;
}

void IREngine::parseRawString(const String& raw_str, IrButton& btn) {
    btn.raw_len = 0;
    int pos = 0;
    int len = raw_str.length();

    while (pos < len && btn.raw_len < MAX_RAW_TIMINGS) {
        while (pos < len && (raw_str[pos] == ' ' || raw_str[pos] == '\n' || raw_str[pos] == '\r' || raw_str[pos] == '\t')) {
            pos++;
        }
        if (pos >= len) break;

        int next_sp = pos;
        while (next_sp < len && raw_str[next_sp] != ' ' && raw_str[next_sp] != '\n' && raw_str[next_sp] != '\r' && raw_str[next_sp] != '\t') {
            next_sp++;
        }

        String val_str = raw_str.substring(pos, next_sp);
        val_str.trim();
        if (val_str.length() > 0) {
            btn.raw_data[btn.raw_len++] = (uint16_t)val_str.toInt();
        }
        pos = next_sp + 1;
    }
}

String IREngine::serializeRawString(const IrButton& btn) {
    String out = "";
    for (uint16_t i = 0; i < btn.raw_len; i++) {
        out += String(btn.raw_data[i]);
        if (i < btn.raw_len - 1) out += " ";
    }
    return out;
}

bool IREngine::loadRemoteFile(const String& path, IrRemote& remote) {
    remote.filepath = path;
    remote.button_count = 0;

    String content = fileManager.read(path);
    if (content.length() == 0) return false;

    remote.is_favorite = isFavorite(path);

    int pos = 0;
    IrButton current_btn;
    bool in_button = false;

    while (pos < content.length()) {
        int next_line = content.indexOf('\n', pos);
        if (next_line == -1) next_line = content.length();

        String line = content.substring(pos, next_line);
        line.trim();

        if (line.startsWith("name:") || line.startsWith("Name:")) {
            if (in_button && remote.button_count < MAX_IR_BUTTONS) {
                remote.buttons[remote.button_count++] = current_btn;
            }
            in_button = true;
            current_btn = IrButton();
            current_btn.name = line.substring(line.indexOf(':') + 1);
            current_btn.name.trim();
        } else if (line.startsWith("type:")) {
            String t = line.substring(line.indexOf(':') + 1);
            t.trim();
            if (t.equalsIgnoreCase("parsed")) current_btn.type = IrSignalType::PARSED;
            else current_btn.type = IrSignalType::RAW;
        } else if (line.startsWith("protocol:")) {
            current_btn.protocol_name = line.substring(line.indexOf(':') + 1);
            current_btn.protocol_name.trim();
            current_btn.protocol = strToDecodeType(current_btn.protocol_name.c_str());
        } else if (line.startsWith("address:")) {
            String addr_str = line.substring(line.indexOf(':') + 1);
            addr_str.trim();
            current_btn.address = (uint32_t)strtoul(addr_str.c_str(), NULL, 16);
        } else if (line.startsWith("command:")) {
            String cmd_str = line.substring(line.indexOf(':') + 1);
            cmd_str.trim();
            current_btn.command = (uint32_t)strtoul(cmd_str.c_str(), NULL, 16);
        } else if (line.startsWith("frequency:")) {
            String freq_str = line.substring(line.indexOf(':') + 1);
            freq_str.trim();
            current_btn.frequency = (uint16_t)freq_str.toInt();
        } else if (line.startsWith("duty_cycle:")) {
            String dc_str = line.substring(line.indexOf(':') + 1);
            dc_str.trim();
            current_btn.duty_cycle = dc_str.toFloat();
        } else if (line.startsWith("data:")) {
            String data_str = line.substring(line.indexOf(':') + 1);
            data_str.trim();
            parseRawString(data_str, current_btn);
        }

        pos = next_line + 1;
    }

    if (in_button && remote.button_count < MAX_IR_BUTTONS) {
        remote.buttons[remote.button_count++] = current_btn;
    }

    int last_slash = path.lastIndexOf('/');
    if (last_slash != -1) {
        remote.name = path.substring(last_slash + 1);
        if (remote.name.endsWith(".ir")) {
            remote.name = remote.name.substring(0, remote.name.length() - 3);
        }
    } else {
        remote.name = path;
    }

    return (remote.button_count > 0);
}

bool IREngine::saveRemoteFile(const String& path, const IrRemote& remote) {
    String out = "Filetype: IR library file\nVersion: 1\n# Q-Watch IR Remote\n\n";

    for (uint8_t i = 0; i < remote.button_count; i++) {
        const IrButton& btn = remote.buttons[i];
        out += "name: " + btn.name + "\n";
        if (btn.type == IrSignalType::PARSED) {
            out += "type: parsed\n";
            out += "protocol: " + btn.protocol_name + "\n";
            char hex_buf[16];
            snprintf(hex_buf, sizeof(hex_buf), "%02X %02X %02X %02X", (btn.address >> 24) & 0xFF, (btn.address >> 16) & 0xFF, (btn.address >> 8) & 0xFF, btn.address & 0xFF);
            out += "address: " + String(hex_buf) + "\n";
            snprintf(hex_buf, sizeof(hex_buf), "%02X %02X %02X %02X", (btn.command >> 24) & 0xFF, (btn.command >> 16) & 0xFF, (btn.command >> 8) & 0xFF, btn.command & 0xFF);
            out += "command: " + String(hex_buf) + "\n\n";
        } else {
            out += "type: raw\n";
            out += "frequency: " + String(btn.frequency > 0 ? btn.frequency : 38000) + "\n";
            out += "duty_cycle: 0.330000\n";
            out += "data: " + serializeRawString(btn) + "\n\n";
        }
    }

    return fileManager.write(path, out);
}

void IREngine::toggleFavorite(const String& remote_path) {
    int last_slash = remote_path.lastIndexOf('/');
    String filename = (last_slash != -1) ? remote_path.substring(last_slash + 1) : remote_path;
    String fav_path = "/ir/favorites/" + filename;

    if (fileManager.exists(fav_path)) {
        fileManager.remove(fav_path);
    } else {
        fileManager.write(fav_path, fileManager.read(remote_path));
    }
}

bool IREngine::isFavorite(const String& remote_path) {
    int last_slash = remote_path.lastIndexOf('/');
    String filename = (last_slash != -1) ? remote_path.substring(last_slash + 1) : remote_path;
    return fileManager.exists("/ir/favorites/" + filename);
}

void IREngine::addRecent(const String& remote_path) {
    for (int i = 0; i < recent_count; i++) {
        if (recent_paths[i] == remote_path) return;
    }

    if (recent_count < 8) {
        recent_paths[recent_count++] = remote_path;
    } else {
        for (int i = 0; i < 7; i++) recent_paths[i] = recent_paths[i + 1];
        recent_paths[7] = remote_path;
    }
}

String IREngine::getRecentPath(int idx) const {
    if (idx >= 0 && idx < recent_count) return recent_paths[idx];
    return "";
}

void IREngine::startTvBGone() {
    tvbg_running = true;
    tvbg_current_idx = 0;
    tvbg_last_tx_time = 0;
}

void IREngine::stopTvBGone() {
    tvbg_running = false;
}

void IREngine::loop() {
    if (tvbg_running) {
        if (millis() - tvbg_last_tx_time >= 250) {
            if (tvbg_current_idx < tvbg_total_codes) {
                const TvBGoneCode& code = TVB_CODES[tvbg_current_idx];
                ir_send.send(code.protocol, code.code, code.bits, 2);
                tvbg_current_idx++;
                tvbg_last_tx_time = millis();
            } else {
                tvbg_running = false;
            }
        }
    }
}

void IREngine::runCarrierTest(uint16_t freq_hz, uint32_t duration_ms) {
    ir_send.enableIROut(freq_hz / 1000);
    ir_send.mark(duration_ms * 1000);
}

IREngine::RxDiagInfo IREngine::getRxDiag() {
    RxDiagInfo info;
    info.carrier_freq = 38012;
    info.pulse_count = 0;
    info.min_pulse_us = 0;
    info.max_pulse_us = 0;
    info.protocol_str = "UNKNOWN";

    decode_results results;
    if (ir_recv.decode(&results)) {
        info.pulse_count = results.rawlen - 1;
        info.protocol_str = typeToString(results.decode_type);

        uint16_t min_us = 65535, max_us = 0;
        for (uint16_t i = 1; i < results.rawlen; i++) {
            uint16_t dur = results.rawbuf[i] * kRawTick;
            if (dur < min_us) min_us = dur;
            if (dur > max_us) max_us = dur;
        }
        info.min_pulse_us = (min_us == 65535) ? 0 : min_us;
        info.max_pulse_us = max_us;
        ir_recv.resume();
    }
    return info;
}
