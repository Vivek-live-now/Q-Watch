#include "ir_engine.h"
#include "hw_config.h"
#include "file_manager.h"

IREngine irEngine;

// Default TV-B-Gone codes table
static const TvBGoneCode DEFAULT_TV_POWER_CODES[] = {
    {"SAMSUNG", SAMSUNG, 0xE0E040BF, 32, 38000},
    {"LG", NEC, 0x20DF10EF, 32, 38000},
    {"SONY 12", SONY, 0xA90, 12, 40000},
    {"SONY 15", SONY, 0xA90, 15, 40000},
    {"SONY 20", SONY, 0xA90, 20, 40000},
    {"PANASONIC", PANASONIC, 0x100BCBD, 48, 37000},
    {"TOSHIBA", NEC, 0x2FD48B7, 32, 38000},
    {"SHARP", SHARP, 0x41A2, 15, 38000},
    {"VIZIO", NEC, 0x20DF10EF, 32, 38000},
    {"PHILIPS RC5", RC5, 0x12, 12, 36000},
    {"PHILIPS RC6", RC6, 0x1000C, 20, 36000},
    {"NEC GENERIC", NEC, 0x00FF00FF, 32, 38000},
    {"JVC", JVC, 0xF123, 16, 38000},
    {"DENON", DENON, 0x2A4, 15, 38000}
};
static const size_t DEFAULT_TV_POWER_CODES_COUNT = sizeof(DEFAULT_TV_POWER_CODES) / sizeof(DEFAULT_TV_POWER_CODES[0]);

IREngine::IREngine() :
    irsend(IR_TX),
    irrecv(IR_RX, 1024, 50, true),
    capturing(false),
    tv_bgone_running(false),
    tv_bgone_idx(0),
    tv_bgone_total(0),
    tv_bgone_last_tx(0),
    carrier_test_active(false),
    carrier_freq(38000) {
}

void IREngine::begin() {
    irsend.begin();
    ensureIrDirectory();
    loadDefaultTvBGoneCodes();
}

void IREngine::ensureIrDirectory() {
    if (!fileManager.exists("/ir")) fileManager.create("/ir/.keep");
    if (!fileManager.exists("/ir/universal")) fileManager.create("/ir/universal/.keep");
}

void IREngine::loadDefaultTvBGoneCodes() {
    tv_bgone_codes.clear();
    for (size_t i = 0; i < DEFAULT_TV_POWER_CODES_COUNT; i++) {
        tv_bgone_codes.push_back(DEFAULT_TV_POWER_CODES[i]);
    }

    if (fileManager.exists("/ir/tvbgone.ir")) {
        IrRemoteFile tv_file;
        if (parseIrFile("/ir/tvbgone.ir", tv_file)) {
            for (size_t i = 0; i < tv_file.buttons.size(); i++) {
                const IrButton& b = tv_file.buttons[i];
                TvBGoneCode code;
                code.brand = strdup(b.name.c_str());
                code.type = strToDecodeType(b.protocol);
                code.data = b.command;
                code.nbits = b.nbits > 0 ? b.nbits : 32;
                code.frequency = b.frequency > 0 ? b.frequency : 38000;
                tv_bgone_codes.push_back(code);
            }
        }
    }

    tv_bgone_total = tv_bgone_codes.size();
}

uint32_t IREngine::parseFlipperHexBytes(const String& val) {
    uint32_t result = 0;
    int tok_pos = 0;
    int byte_idx = 0;

    while (tok_pos < val.length() && byte_idx < 4) {
        int space_idx = val.indexOf(' ', tok_pos);
        if (space_idx < 0) space_idx = val.length();
        String byteStr = val.substring(tok_pos, space_idx);
        byteStr.trim();
        if (byteStr.length() > 0) {
            uint8_t b = (uint8_t)strtoul(byteStr.c_str(), nullptr, 16);
            result |= ((uint32_t)b << (byte_idx * 8));
            byte_idx++;
        }
        tok_pos = space_idx + 1;
    }
    return result;
}

decode_type_t IREngine::strToDecodeType(const String& proto) {
    String p = proto;
    p.trim();

    if (p == "NEC" || p == "NECext" || p == "NEC42") return NEC;
    if (p == "Samsung32" || p == "SAMSUNG") return SAMSUNG;
    if (p == "Sony" || p == "SIRC" || p == "SIRC15" || p == "SIRC20") return SONY;
    if (p == "RC5" || p == "RC5X") return RC5;
    if (p == "RC6") return RC6;
    if (p == "Panasonic") return PANASONIC;
    if (p == "LG" || p == "LG2") return LG;
    if (p == "JVC") return JVC;
    if (p == "Sharp") return SHARP;
    if (p == "Denon") return DENON;

    return UNKNOWN;
}

String IREngine::decodeTypeToStr(decode_type_t type, uint16_t nbits) {
    switch (type) {
        case NEC:
            if (nbits == 42) return "NEC42";
            return "NECext";
        case SAMSUNG: return "Samsung32";
        case SONY:
            if (nbits == 15) return "SIRC15";
            if (nbits == 20) return "SIRC20";
            return "SIRC";
        case RC5: return "RC5";
        case RC5X: return "RC5X";
        case RC6: return "RC6";
        case PANASONIC: return "Panasonic";
        case LG: return "LG";
        case JVC: return "JVC";
        case SHARP: return "Sharp";
        case DENON: return "Denon";
        default: return "UNKNOWN";
    }
}

bool IREngine::sendParsed(const String& protocol, uint32_t address, uint32_t command, uint16_t nbits) {
    decode_type_t type = strToDecodeType(protocol);

    if (type == NEC) {
        uint64_t data = irsend.encodeNEC(address, command);
        irsend.sendNEC(data, nbits > 0 ? nbits : 32);
        return true;
    } else if (type == SAMSUNG) {
        uint64_t data = irsend.encodeSAMSUNG(address, command);
        irsend.sendSAMSUNG(data, nbits > 0 ? nbits : 32);
        return true;
    } else if (type == SONY) {
        uint16_t bits = (protocol == "SIRC15") ? 15 : ((protocol == "SIRC20") ? 20 : (nbits > 0 ? nbits : 12));
        uint64_t data = irsend.encodeSony(bits, command, address);
        irsend.sendSony(data, bits);
        return true;
    } else if (type == RC5) {
        uint64_t data = (protocol == "RC5X") ? irsend.encodeRC5X(address, command) : irsend.encodeRC5(address, command);
        irsend.sendRC5(data, nbits > 0 ? nbits : 12);
        return true;
    } else if (type == RC6) {
        uint64_t data = irsend.encodeRC6(address, command);
        irsend.sendRC6(data, nbits > 0 ? nbits : 20);
        return true;
    } else if (type != UNKNOWN) {
        irsend.send(type, command, nbits > 0 ? nbits : 32);
        return true;
    }
    return false;
}

bool IREngine::sendRaw(const uint16_t* timings, size_t count, uint32_t frequency) {
    if (!timings || count == 0) return false;
    size_t send_count = min(count, MAX_IR_RAW_TIMINGS);
    uint16_t khz = (frequency > 1000) ? (frequency / 1000) : (frequency > 0 ? frequency : 38);
    irsend.sendRaw(timings, send_count, khz);
    return true;
}

bool IREngine::sendButton(const IrButton& btn) {
    if (capturing) {
        stopCapture();
    }

    if (btn.type == IrSignalType::PARSED) {
        return sendParsed(btn.protocol, btn.address, btn.command, btn.nbits);
    } else {
        if (btn.raw_data.empty()) return false;
        return sendRaw(btn.raw_data.data(), btn.raw_data.size(), btn.frequency);
    }
}

void IREngine::startCapture() {
    irrecv.enableIRIn();
    capturing = true;
}

void IREngine::stopCapture() {
    irrecv.disableIRIn();
    capturing = false;
}

bool IREngine::checkCapturedSignal(IrButton& out_btn) {
    if (!capturing) return false;

    if (irrecv.decode(&results)) {
        out_btn.name = "Captured";
        out_btn.raw_data.clear();
        out_btn.truncated = false;

        if (results.decode_type != UNKNOWN) {
            out_btn.type = IrSignalType::PARSED;
            out_btn.protocol = decodeTypeToStr(results.decode_type, results.bits);
            out_btn.command = results.value & 0xFFFFFFFF;
            out_btn.address = results.address;
            out_btn.nbits = results.bits;
            out_btn.frequency = 0; // 0 = unknown/unmeasured carrier
            out_btn.duty_cycle = 0.0f;
            out_btn.has_duty_cycle = false;
        } else {
            out_btn.type = IrSignalType::RAW;
            out_btn.protocol = "RAW";
            out_btn.address = 0;
            out_btn.command = 0;
            out_btn.nbits = 0;
            out_btn.frequency = 0; // 0 = unknown carrier
            out_btn.duty_cycle = 0.0f;
            out_btn.has_duty_cycle = false;

            uint16_t* raw_arr = resultToRawArray(&results);
            uint16_t raw_len = getCorrectedRawLength(&results);

            if (raw_len > MAX_IR_RAW_TIMINGS) {
                raw_len = MAX_IR_RAW_TIMINGS;
                out_btn.truncated = true;
            }

            for (uint16_t i = 0; i < raw_len; i++) {
                out_btn.raw_data.push_back(raw_arr[i]);
            }
            delete[] raw_arr;
        }

        irrecv.resume();
        return true;
    }
    return false;
}

bool IREngine::analyzeRawToParsed(IrButton& btn) {
    if (btn.type != IrSignalType::RAW || btn.raw_data.empty()) return false;

    decode_results test_results;
    uint16_t tick_count = min((size_t)btn.raw_data.size(), (size_t)1024);

    // Allocate temporary rawbuf for decode_results (RAWTICK = 2us by default in IRremoteESP8266)
    test_results.rawlen = tick_count + 1;
    test_results.rawbuf = new uint16_t[test_results.rawlen];
    test_results.rawbuf[0] = 0;

    for (size_t i = 0; i < tick_count; i++) {
        test_results.rawbuf[i + 1] = btn.raw_data[i] / kRawTick;
    }

    bool decoded = false;
    if (irrecv.decode(&test_results)) {
        if (test_results.decode_type != UNKNOWN) {
            btn.type = IrSignalType::PARSED;
            btn.protocol = decodeTypeToStr(test_results.decode_type, test_results.bits);
            btn.command = test_results.value & 0xFFFFFFFF;
            btn.address = test_results.address;
            btn.nbits = test_results.bits;
            decoded = true;
        }
    }

    delete[] test_results.rawbuf;
    return decoded;
}

bool IREngine::parseIrFile(const String& path, IrRemoteFile& remote) {
    if (!fileManager.exists(path)) return false;

    String content = fileManager.read(path);
    if (content.length() == 0) return false;

    remote.filepath = path;
    int slash_idx = path.lastIndexOf('/');
    remote.name = (slash_idx >= 0) ? path.substring(slash_idx + 1) : path;
    if (remote.name.endsWith(".ir")) {
        remote.name = remote.name.substring(0, remote.name.length() - 3);
    }
    remote.buttons.clear();

    IrButton current_btn;
    bool in_button = false;

    int pos = 0;
    while (pos < content.length()) {
        int next_nl = content.indexOf('\n', pos);
        if (next_nl < 0) next_nl = content.length();

        String line = content.substring(pos, next_nl);
        line.trim();
        pos = next_nl + 1;

        if (line.startsWith("#") || line.length() == 0) continue;

        int colon = line.indexOf(':');
        if (colon < 0) continue;

        String key = line.substring(0, colon);
        String val = line.substring(colon + 1);
        key.trim();
        val.trim();

        if (key == "name") {
            if (in_button) {
                remote.buttons.push_back(current_btn);
            }
            current_btn = IrButton();
            current_btn.name = val;
            current_btn.frequency = 0;
            current_btn.duty_cycle = 0.0f;
            current_btn.has_duty_cycle = false;
            in_button = true;
        } else if (key == "type") {
            current_btn.type = (val == "raw") ? IrSignalType::RAW : IrSignalType::PARSED;
        } else if (key == "protocol") {
            current_btn.protocol = val;
        } else if (key == "address") {
            current_btn.address = parseFlipperHexBytes(val);
        } else if (key == "command") {
            current_btn.command = parseFlipperHexBytes(val);
        } else if (key == "frequency") {
            current_btn.frequency = val.toInt();
        } else if (key == "duty_cycle") {
            current_btn.duty_cycle = val.toFloat();
            current_btn.has_duty_cycle = true;
        } else if (key == "data") {
            current_btn.raw_data.clear();
            int tok_pos = 0;
            while (tok_pos < val.length()) {
                int space_idx = val.indexOf(' ', tok_pos);
                if (space_idx < 0) space_idx = val.length();
                String numStr = val.substring(tok_pos, space_idx);
                numStr.trim();
                if (numStr.length() > 0) {
                    if (current_btn.raw_data.size() < MAX_IR_RAW_TIMINGS) {
                        current_btn.raw_data.push_back((uint16_t)numStr.toInt());
                    } else {
                        current_btn.truncated = true;
                    }
                }
                tok_pos = space_idx + 1;
            }
        }
    }

    if (in_button) {
        remote.buttons.push_back(current_btn);
    }

    return !remote.buttons.empty();
}

bool IREngine::saveIrFile(const String& path, const IrRemoteFile& remote) {
    String out = "Filetype: IR library file\nVersion: 1\n#\n";

    for (size_t i = 0; i < remote.buttons.size(); i++) {
        const IrButton& b = remote.buttons[i];
        out += "name: " + b.name + "\n";
        if (b.type == IrSignalType::PARSED) {
            out += "type: parsed\n";
            out += "protocol: " + (b.protocol.length() > 0 ? b.protocol : "NECext") + "\n";

            char hexBuf[32];
            snprintf(hexBuf, sizeof(hexBuf), "%02X %02X %02X %02X",
                     (uint8_t)(b.address & 0xFF), (uint8_t)((b.address >> 8) & 0xFF),
                     (uint8_t)((b.address >> 16) & 0xFF), (uint8_t)((b.address >> 24) & 0xFF));
            out += "address: " + String(hexBuf) + "\n";

            snprintf(hexBuf, sizeof(hexBuf), "%02X %02X %02X %02X",
                     (uint8_t)(b.command & 0xFF), (uint8_t)((b.command >> 8) & 0xFF),
                     (uint8_t)((b.command >> 16) & 0xFF), (uint8_t)((b.command >> 24) & 0xFF));
            out += "command: " + String(hexBuf) + "\n#\n";
        } else {
            out += "type: raw\n";
            if (b.frequency > 0) {
                out += "frequency: " + String(b.frequency) + "\n";
            }
            if (b.has_duty_cycle) {
                out += "duty_cycle: " + String(b.duty_cycle, 6) + "\n";
            }
            out += "data:";
            size_t write_count = min(b.raw_data.size(), MAX_IR_RAW_TIMINGS);
            for (size_t j = 0; j < write_count; j++) {
                out += " " + String(b.raw_data[j]);
            }
            out += "\n#\n";
        }
    }

    return fileManager.write(path, out);
}

bool IREngine::appendButtonToIrFile(const String& path, const IrButton& btn) {
    IrRemoteFile remote;
    if (fileManager.exists(path)) {
        parseIrFile(path, remote);
    } else {
        remote.filepath = path;
        int slash = path.lastIndexOf('/');
        remote.name = (slash >= 0) ? path.substring(slash + 1) : path;
        if (remote.name.endsWith(".ir")) remote.name = remote.name.substring(0, remote.name.length() - 3);
    }

    remote.buttons.push_back(btn);
    return saveIrFile(path, remote);
}

std::vector<String> IREngine::listIrFiles() {
    std::vector<String> files;
    FileInfo entries[32];
    size_t count = fileManager.listDir("/ir", entries, 32);

    for (size_t i = 0; i < count; i++) {
        if (!entries[i].isDir && entries[i].name.endsWith(".ir")) {
            files.push_back("/ir/" + entries[i].name);
        }
    }
    return files;
}

void IREngine::startTvBGone() {
    if (tv_bgone_codes.empty()) loadDefaultTvBGoneCodes();
    tv_bgone_idx = 0;
    tv_bgone_last_tx = 0;
    tv_bgone_running = true;
}

void IREngine::stopTvBGone() {
    tv_bgone_running = false;
}

int IREngine::getTvBGoneProgressPercent() const {
    if (tv_bgone_total == 0) return 0;
    return (tv_bgone_idx * 100) / tv_bgone_total;
}

void IREngine::loop() {
    if (tv_bgone_running) {
        if (millis() - tv_bgone_last_tx >= 200) {
            if (tv_bgone_idx < tv_bgone_codes.size()) {
                const TvBGoneCode& code = tv_bgone_codes[tv_bgone_idx];
                tv_bgone_current_brand = code.brand;
                irsend.send(code.type, code.data, code.nbits);
                tv_bgone_last_tx = millis();
                tv_bgone_idx++;
            } else {
                tv_bgone_running = false;
            }
        }
    }
}

void IREngine::addRecent(const String& remote_name, const IrButton& btn) {
    String entry = remote_name + ":" + btn.name;

    std::vector<String> current = getRecentList();
    std::vector<String> updated;
    updated.push_back(entry);

    for (size_t i = 0; i < current.size() && updated.size() < 10; i++) {
        if (current[i] != entry) {
            updated.push_back(current[i]);
        }
    }

    String out = "";
    for (size_t i = 0; i < updated.size(); i++) {
        out += updated[i] + "\n";
    }
    fileManager.write("/ir/recent.txt", out);
}

std::vector<String> IREngine::getRecentList() {
    std::vector<String> list;
    if (!fileManager.exists("/ir/recent.txt")) return list;

    String content = fileManager.read("/ir/recent.txt");
    int pos = 0;
    while (pos < content.length()) {
        int nl = content.indexOf('\n', pos);
        if (nl < 0) nl = content.length();
        String line = content.substring(pos, nl);
        line.trim();
        if (line.length() > 0) list.push_back(line);
        pos = nl + 1;
    }
    return list;
}

void IREngine::addFavorite(const String& remote_name, const String& button_name) {
    String entry = remote_name + ":" + button_name;
    if (isFavorite(remote_name, button_name)) return;

    fileManager.append("/ir/favorites.txt", entry + "\n");
}

void IREngine::removeFavorite(const String& remote_name, const String& button_name) {
    String entry = remote_name + ":" + button_name;
    std::vector<String> favs = getFavoritesList();

    String out = "";
    for (size_t i = 0; i < favs.size(); i++) {
        if (favs[i] != entry) {
            out += favs[i] + "\n";
        }
    }
    fileManager.write("/ir/favorites.txt", out);
}

bool IREngine::isFavorite(const String& remote_name, const String& button_name) {
    String entry = remote_name + ":" + button_name;
    std::vector<String> favs = getFavoritesList();
    for (size_t i = 0; i < favs.size(); i++) {
        if (favs[i] == entry) return true;
    }
    return false;
}

std::vector<String> IREngine::getFavoritesList() {
    std::vector<String> list;
    if (!fileManager.exists("/ir/favorites.txt")) return list;

    String content = fileManager.read("/ir/favorites.txt");
    int pos = 0;
    while (pos < content.length()) {
        int nl = content.indexOf('\n', pos);
        if (nl < 0) nl = content.length();
        String line = content.substring(pos, nl);
        line.trim();
        if (line.length() > 0) list.push_back(line);
        pos = nl + 1;
    }
    return list;
}

void IREngine::startCarrierTest(uint32_t freq_hz) {
    carrier_freq = freq_hz;
    carrier_test_active = true;

    ledcSetup(7, freq_hz, 8);
    ledcAttachPin(IR_TX, 7);
    ledcWrite(7, 85);
}

void IREngine::stopCarrierTest() {
    carrier_test_active = false;
    ledcWrite(7, 0);
    ledcDetachPin(IR_TX);
    pinMode(IR_TX, OUTPUT);
    digitalWrite(IR_TX, LOW);
}
