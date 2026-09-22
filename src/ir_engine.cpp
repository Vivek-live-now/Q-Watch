#include "ir_engine.h"
#include "hw_config.h"
#include "file_manager.h"
#include <ArduinoJson.h>

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
    if (!fileManager.exists("/ir")) {
        fileManager.create("/ir/.keep");
    }
    if (!fileManager.exists("/ir/universal")) {
        fileManager.create("/ir/universal/.keep");
    }

    if (!fileManager.exists("/ir/Samsung_TV.ir")) {
        IrRemoteFile samsung;
        samsung.name = "Samsung_TV";
        samsung.filepath = "/ir/Samsung_TV.ir";

        IrButton b1; b1.name = "Power"; b1.type = IrSignalType::PARSED; b1.protocol = "Samsung32"; b1.address = 0xE0E0; b1.command = 0x40BF; samsung.buttons.push_back(b1);
        IrButton b2; b2.name = "Vol+"; b2.type = IrSignalType::PARSED; b2.protocol = "Samsung32"; b2.address = 0xE0E0; b2.command = 0xE0E0; samsung.buttons.push_back(b2);
        IrButton b3; b3.name = "Vol-"; b3.type = IrSignalType::PARSED; b3.protocol = "Samsung32"; b3.address = 0xE0E0; b3.command = 0xD0E0; samsung.buttons.push_back(b3);
        IrButton b4; b4.name = "Mute"; b4.type = IrSignalType::PARSED; b4.protocol = "Samsung32"; b4.address = 0xE0E0; b4.command = 0xF0E0; samsung.buttons.push_back(b4);

        saveIrFile("/ir/Samsung_TV.ir", samsung);
    }

    if (!fileManager.exists("/ir/LG_TV.ir")) {
        IrRemoteFile lg;
        lg.name = "LG_TV";
        lg.filepath = "/ir/LG_TV.ir";

        IrButton b1; b1.name = "Power"; b1.type = IrSignalType::PARSED; b1.protocol = "NEC"; b1.address = 0x20DF; b1.command = 0x10EF; lg.buttons.push_back(b1);
        IrButton b2; b2.name = "Vol+"; b2.type = IrSignalType::PARSED; b2.protocol = "NEC"; b2.address = 0x20DF; b2.command = 0x40BF; lg.buttons.push_back(b2);
        IrButton b3; b3.name = "Vol-"; b3.type = IrSignalType::PARSED; b3.protocol = "NEC"; b3.address = 0x20DF; b3.command = 0xC03F; lg.buttons.push_back(b3);

        saveIrFile("/ir/LG_TV.ir", lg);
    }

    if (!fileManager.exists("/ir/universal/TV.ir")) {
        IrRemoteFile uni;
        uni.name = "Universal_TV";
        uni.filepath = "/ir/universal/TV.ir";

        IrButton b1; b1.name = "Samsung Pwr"; b1.type = IrSignalType::PARSED; b1.protocol = "Samsung32"; b1.address = 0xE0E0; b1.command = 0x40BF; uni.buttons.push_back(b1);
        IrButton b2; b2.name = "LG Pwr"; b2.type = IrSignalType::PARSED; b2.protocol = "NEC"; b2.address = 0x20DF; b2.command = 0x10EF; uni.buttons.push_back(b2);
        IrButton b3; b3.name = "Sony Pwr"; b3.type = IrSignalType::PARSED; b3.protocol = "Sony"; b3.address = 0x0001; b3.command = 0x00A9; uni.buttons.push_back(b3);

        saveIrFile("/ir/universal/TV.ir", uni);
    }
}

void IREngine::loadDefaultTvBGoneCodes() {
    tv_bgone_codes.clear();
    for (size_t i = 0; i < DEFAULT_TV_POWER_CODES_COUNT; i++) {
        tv_bgone_codes.push_back(DEFAULT_TV_POWER_CODES[i]);
    }
    tv_bgone_total = tv_bgone_codes.size();
}

decode_type_t IREngine::strToDecodeType(const String& proto) {
    String p = proto;
    p.toUpperCase();
    p.trim();

    if (p == "NEC" || p == "NECEXT" || p == "NEC42") return NEC;
    if (p == "SAMSUNG" || p == "SAMSUNG32") return SAMSUNG;
    if (p == "SONY" || p == "SONY12" || p == "SONY15" || p == "SONY20") return SONY;
    if (p == "RC5" || p == "RC5X") return RC5;
    if (p == "RC6") return RC6;
    if (p == "PANASONIC") return PANASONIC;
    if (p == "LG" || p == "LG2") return LG;
    if (p == "JVC") return JVC;
    if (p == "SHARP") return SHARP;
    if (p == "DENON") return DENON;

    return UNKNOWN;
}

String IREngine::decodeTypeToStr(decode_type_t type) {
    switch (type) {
        case NEC: return "NEC";
        case SAMSUNG: return "Samsung32";
        case SONY: return "Sony";
        case RC5: return "RC5";
        case RC6: return "RC6";
        case PANASONIC: return "Panasonic";
        case LG: return "LG";
        case JVC: return "JVC";
        case SHARP: return "Sharp";
        case DENON: return "Denon";
        default: return "UNKNOWN";
    }
}

bool IREngine::sendParsed(const String& protocol, uint64_t address, uint64_t command, uint16_t nbits) {
    decode_type_t type = strToDecodeType(protocol);
    if (type != UNKNOWN) {
        irsend.send(type, command, nbits > 0 ? nbits : 32);
        return true;
    }
    return false;
}

bool IREngine::sendRaw(const uint16_t* timings, size_t count, uint16_t frequency) {
    if (!timings || count == 0) return false;
    uint16_t khz = frequency > 1000 ? frequency / 1000 : (frequency > 0 ? frequency : 38);
    irsend.sendRaw(timings, count, khz);
    return true;
}

bool IREngine::sendButton(const IrButton& btn) {
    if (capturing) {
        stopCapture();
    }

    if (btn.type == IrSignalType::PARSED) {
        uint64_t val = btn.command;
        if (val == 0 && btn.address != 0) val = btn.address;
        uint16_t bits = btn.nbits > 0 ? btn.nbits : 32;
        return sendParsed(btn.protocol, btn.address, val, bits);
    } else {
        if (btn.raw_data.empty()) return false;
        uint16_t khz = btn.frequency > 1000 ? btn.frequency / 1000 : 38;
        irsend.sendRaw(btn.raw_data.data(), btn.raw_data.size(), khz);
        return true;
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

        if (results.decode_type != UNKNOWN) {
            out_btn.type = IrSignalType::PARSED;
            out_btn.protocol = decodeTypeToStr(results.decode_type);
            out_btn.command = results.value;
            out_btn.address = results.address;
            out_btn.nbits = results.bits;
            out_btn.frequency = 38000;
            out_btn.duty_cycle = 0.33f;
        } else {
            out_btn.type = IrSignalType::RAW;
            out_btn.protocol = "RAW";
            out_btn.address = 0;
            out_btn.command = 0;
            out_btn.nbits = 0;
            out_btn.frequency = 38000;
            out_btn.duty_cycle = 0.33f;

            uint16_t* raw_arr = resultToRawArray(&results);
            uint16_t raw_len = getCorrectedRawLength(&results);
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

    if (btn.raw_data.size() >= 66) {
        if (btn.raw_data[0] >= 8000 && btn.raw_data[0] <= 10000 &&
            btn.raw_data[1] >= 4000 && btn.raw_data[1] <= 5000) {
            btn.type = IrSignalType::PARSED;
            btn.protocol = "NEC";
            btn.nbits = 32;
            btn.address = 0x00FF;
            btn.command = 0x00FF;
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// Flipper Zero / Bruce .ir File Parser & Writer
// ----------------------------------------------------------------------------
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
            current_btn.frequency = 38000;
            current_btn.duty_cycle = 0.33f;
            in_button = true;
        } else if (key == "type") {
            current_btn.type = (val == "raw") ? IrSignalType::RAW : IrSignalType::PARSED;
        } else if (key == "protocol") {
            current_btn.protocol = val;
        } else if (key == "address") {
            uint64_t addr = 0;
            val.replace(" ", "");
            addr = strtoull(val.c_str(), nullptr, 16);
            current_btn.address = addr;
        } else if (key == "command") {
            uint64_t cmd = 0;
            val.replace(" ", "");
            cmd = strtoull(val.c_str(), nullptr, 16);
            current_btn.command = cmd;
        } else if (key == "frequency") {
            current_btn.frequency = val.toInt();
        } else if (key == "duty_cycle") {
            current_btn.duty_cycle = val.toFloat();
        } else if (key == "data") {
            current_btn.raw_data.clear();
            int tok_pos = 0;
            while (tok_pos < val.length()) {
                int space_idx = val.indexOf(' ', tok_pos);
                if (space_idx < 0) space_idx = val.length();
                String numStr = val.substring(tok_pos, space_idx);
                numStr.trim();
                if (numStr.length() > 0) {
                    current_btn.raw_data.push_back((uint16_t)numStr.toInt());
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
            out += "protocol: " + (b.protocol.length() > 0 ? b.protocol : "NEC") + "\n";

            char hexBuf[32];
            snprintf(hexBuf, sizeof(hexBuf), "%02X %02X %02X %02X",
                     (uint8_t)(b.address >> 24), (uint8_t)(b.address >> 16),
                     (uint8_t)(b.address >> 8), (uint8_t)(b.address));
            out += "address: " + String(hexBuf) + "\n";

            snprintf(hexBuf, sizeof(hexBuf), "%02X %02X %02X %02X",
                     (uint8_t)(b.command >> 24), (uint8_t)(b.command >> 16),
                     (uint8_t)(b.command >> 8), (uint8_t)(b.command));
            out += "command: " + String(hexBuf) + "\n#\n";
        } else {
            out += "type: raw\n";
            out += "frequency: " + String(b.frequency > 0 ? b.frequency : 38000) + "\n";
            out += "duty_cycle: 0.330000\n";
            out += "data:";
            for (size_t j = 0; j < b.raw_data.size(); j++) {
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

// ----------------------------------------------------------------------------
// TV-B-Gone Implementation
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Recent & Favorites
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Signal Lab Carrier Test using ESP32 LEDC PWM
// ----------------------------------------------------------------------------
void IREngine::startCarrierTest(uint32_t freq_hz) {
    carrier_freq = freq_hz;
    carrier_test_active = true;

    // Use ESP32 LEDC channel 7 on GPIO 18 for carrier test PWM output
    ledcSetup(7, freq_hz, 8); // 8-bit resolution
    ledcAttachPin(IR_TX, 7);
    ledcWrite(7, 85); // ~33% duty cycle
}

void IREngine::stopCarrierTest() {
    carrier_test_active = false;
    ledcWrite(7, 0);
    ledcDetachPin(IR_TX);
    pinMode(IR_TX, OUTPUT);
    digitalWrite(IR_TX, LOW);
}
