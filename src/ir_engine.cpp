#include "ir_engine.h"
#include "hw_config.h"
#include "file_manager.h"

IREngine irEngine;

// Expanded Global TV-B-Gone codes table (67 power codes across all major brands)
static const TvBGoneCode DEFAULT_TV_POWER_CODES[] = {
    // Top Global Tier
    {"SAMSUNG 1", SAMSUNG, 0xE0E040BF, 32, 38000},
    {"SAMSUNG 2", SAMSUNG, 0xE0E019E6, 32, 38000},
    {"LG 1", NEC, 0x20DF10EF, 32, 38000},
    {"LG 2", NEC, 0x20DF08F7, 32, 38000},
    {"SONY 12B", SONY, 0xA90, 12, 40000},
    {"SONY 15B", SONY, 0xA90, 15, 40000},
    {"SONY 20B", SONY, 0xA90, 20, 40000},
    {"PANASONIC 1", PANASONIC, 0x100BCBD, 48, 37000},
    {"PANASONIC 2", PANASONIC, 0x1008C8D, 48, 37000},
    {"TOSHIBA 1", NEC, 0x2FD48B7, 32, 38000},
    {"TOSHIBA 2", NEC, 0x02FD48B7, 32, 38000},
    {"SHARP 1", SHARP, 0x41A2, 15, 38000},
    {"SHARP 2", SHARP, 0x42A2, 15, 38000},
    {"VIZIO 1", NEC, 0x20DF10EF, 32, 38000},
    {"VIZIO 2", NEC, 0x20DF609F, 32, 38000},
    {"PHILIPS RC5", RC5, 0x12, 12, 36000},
    {"PHILIPS RC6", RC6, 0x1000C, 20, 36000},
    // Modern Smart TV Brands (TCL, Hisense, Xiaomi, Insignia)
    {"TCL 1", NEC, 0x4CB040BF, 32, 38000},
    {"TCL 2", NEC, 0x00FF08F7, 32, 38000},
    {"HISENSE 1", NEC, 0xFDF00F, 32, 38000},
    {"HISENSE 2", NEC, 0xFD08F7, 32, 38000},
    {"INSIGNIA 1", NEC, 0x00FF807F, 32, 38000},
    {"INSIGNIA 2", NEC, 0x20DF10EF, 32, 38000},
    {"XIAOMI MI", NEC, 0x00FF807F, 32, 38000},
    // User-Requested Protocols: RCA & Nikai
    {"RCA 1 (RCA24)", DECODE_TYPE_RCA, 0x040C, 24, 38000}, // Addr 4, Cmd 0x0C
    {"RCA 2 (RCA24)", DECODE_TYPE_RCA, 0x0400, 24, 38000}, // Addr 4, Cmd 0x00
    {"RCA 3 (NEC)", NEC, 0x00FF807F, 32, 38000},
    {"NIKAI 1 (24B)", NIKAI, 0x807F, 24, 38000},
    {"NIKAI 2 (24B)", NIKAI, 0x40BF, 24, 38000},
    {"NIKAI 3 (NEC)", NEC, 0x00FF807F, 32, 38000},
    // Major Regional & Global Brands
    {"SANYO 1", NEC, 0x1FE48B7, 32, 38000},
    {"SANYO 2", NEC, 0x00FF1AE5, 32, 38000},
    {"MITSUBISHI", MITSUBISHI, 0x02FD48B7, 16, 38000},
    {"HITACHI 1", NEC, 0x0AF5807F, 32, 38000},
    {"HITACHI 2", NEC, 0x51AE, 32, 38000},
    {"PIONEER", NEC, 0xA55A38C7, 32, 40000},
    {"JVC", JVC, 0xF123, 16, 38000},
    {"DENON", DENON, 0x2A4, 15, 38000},
    {"SKYWORTH", NEC, 0x02FD00FF, 32, 38000},
    {"HAIER 1", NEC, 0x20DF10EF, 32, 38000},
    {"HAIER 2", NEC, 0x00FF08F7, 32, 38000},
    {"THOMSON", RC5, 0x12, 12, 36000},
    {"GRUNDIG", RC5, 0x12, 12, 36000},
    {"TELEFUNKEN", NEC, 0x4CB040BF, 32, 38000},
    {"BLAUPUNKT", NEC, 0x20DF10EF, 32, 38000},
    {"AKAI", NEC, 0x00FF807F, 32, 38000},
    {"CHANGHONG", NEC, 0x00FF807F, 32, 38000},
    {"SCEPTRE", NEC, 0x00FF807F, 32, 38000},
    {"WESTINGHOUSE", NEC, 0x00FF807F, 32, 38000},
    {"ELEMENT", NEC, 0x00FF807F, 32, 38000},
    {"EMERSON", NEC, 0x00FF807F, 32, 38000},
    {"DAEWOO", NEC, 0x00FF807F, 32, 38000},
    {"AOC", NEC, 0x20DF10EF, 32, 38000},
    {"VIEWSONIC", NEC, 0x20DF10EF, 32, 38000},
    {"SANSUI", NEC, 0x00FF807F, 32, 38000},
    {"FUNAI", NEC, 0x00FF807F, 32, 38000},
    {"MAGNAVOX", NEC, 0x00FF807F, 32, 38000},
    {"ONIDA", NEC, 0x00FF807F, 32, 38000},
    {"VIDEOCON", NEC, 0x00FF807F, 32, 38000},
    {"MICROMAX", NEC, 0x00FF807F, 32, 38000},
    {"BPL", RC5, 0x12, 12, 36000},
    {"POLYTRON", NEC, 0x00FF807F, 32, 38000},
    {"WALTON", NEC, 0x00FF807F, 32, 38000},
    // Universal & Generic Power Codes
    {"GENERIC NEC 1", NEC, 0x00FF00FF, 32, 38000},
    {"GENERIC NEC 2", NEC, 0x00FF807F, 32, 38000},
    {"GENERIC NEC 3", NEC, 0x20DF10EF, 32, 38000},
    {"GENERIC RC5", RC5, 0x12, 12, 36000}
};
static const size_t DEFAULT_TV_POWER_CODES_COUNT = sizeof(DEFAULT_TV_POWER_CODES) / sizeof(DEFAULT_TV_POWER_CODES[0]);

IREngine::IREngine() :
    irsend(IR_TX, false, true),
    irrecv(IR_RX, 1024, 50, true),
    capturing(false),
    calibrated_offset(0),
    polarity_inverted(false),
    last_lab_status("IDLE"),
    tv_bgone_running(false),
    tv_bgone_idx(0),
    tv_bgone_total(0),
    tv_bgone_last_tx(0),
    carrier_test_active(false),
    carrier_freq(38000) {
}

void IREngine::begin() {
    irsend.begin();
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32)
    gpio_set_drive_capability((gpio_num_t)IR_TX, GPIO_DRIVE_CAP_3);
#endif
    runCalibration(38000);
    ensureIrDirectory();
    loadDefaultTvBGoneCodes();
}

int8_t IREngine::runCalibration(uint32_t freq_hz) {
    calibrated_offset = irsend.calibrate(freq_hz > 0 ? freq_hz : 38000);
    return calibrated_offset;
}

void IREngine::setPolarityInverted(bool inv) {
    polarity_inverted = inv;
    irsend = IRsend(IR_TX, polarity_inverted, true);
    irsend.begin();
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32)
    gpio_set_drive_capability((gpio_num_t)IR_TX, GPIO_DRIVE_CAP_3);
#endif
    runCalibration(38000);
}

void IREngine::togglePolarity() {
    setPolarityInverted(!polarity_inverted);
}

void IREngine::ensureIrDirectory() {
    if (!fileManager.exists("/ir")) fileManager.create("/ir/.keep");
    if (!fileManager.exists("/ir/universal")) fileManager.create("/ir/universal/.keep");
}

void IREngine::loadDefaultTvBGoneCodes() {
    tv_bgone_codes.clear();
    tv_bgone_codes.reserve(DEFAULT_TV_POWER_CODES_COUNT + 16);
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

    if (p.equalsIgnoreCase("NEC") || p.equalsIgnoreCase("NECext") || p.equalsIgnoreCase("NEC42")) return NEC;
    if (p.equalsIgnoreCase("Samsung32") || p.equalsIgnoreCase("Samsung")) return SAMSUNG;
    if (p.equalsIgnoreCase("SIRC") || p.equalsIgnoreCase("SIRC15") || p.equalsIgnoreCase("SIRC20") || p.equalsIgnoreCase("Sony")) return SONY;
    if (p.equalsIgnoreCase("RC5") || p.equalsIgnoreCase("RC5X")) return RC5;
    if (p.equalsIgnoreCase("RC6")) return RC6;
    if (p.equalsIgnoreCase("NIKAI")) return NIKAI;
    if (p.equalsIgnoreCase("RCA")) return DECODE_TYPE_RCA;

    return ::strToDecodeType(p.c_str());
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
        case RC5X: return "RC5X";
        case NIKAI: return "NIKAI";
        case DECODE_TYPE_RCA: return "RCA";
        default: return typeToString(type);
    }
}

uint16_t IREngine::getProtocolDefaultBits(const String& proto) {
    String p = proto;
    p.trim();
    if (p.equalsIgnoreCase("RCA") || p.equalsIgnoreCase("NIKAI")) return 24;
    if (p.equalsIgnoreCase("SONY") || p.equalsIgnoreCase("SIRC")) return 12;
    if (p.equalsIgnoreCase("SIRC15")) return 15;
    if (p.equalsIgnoreCase("SIRC20")) return 20;
    if (p.equalsIgnoreCase("RC5")) return 12;
    if (p.equalsIgnoreCase("RC5X")) return 13;
    if (p.equalsIgnoreCase("RC6")) return 20;
    if (p.equalsIgnoreCase("SHARP") || p.equalsIgnoreCase("DENON")) return 15;
    if (p.equalsIgnoreCase("JVC")) return 16;
    if (p.equalsIgnoreCase("PANASONIC")) return 48;
    if (p.equalsIgnoreCase("NEC42")) return 42;
    return 32;
}

bool IREngine::sendRCA(uint32_t address, uint32_t command, uint16_t repeats) {
    uint32_t addr = address & 0x0F;
    uint32_t cmd = command & 0xFF;
    uint32_t inv_addr = (~addr) & 0x0F;
    uint32_t inv_cmd = (~cmd) & 0xFF;
    uint32_t data = addr | (cmd << 4) | (inv_addr << 12) | (inv_cmd << 16);

    irsend.sendGeneric(
        4000, 4000, // preamble mark, space
        500, 2000,  // bit1 mark, space
        500, 1000,  // bit0 mark, space
        500, 8000,  // footer mark, min gap
        data, 24,   // 24 bits
        38,         // 38 kHz carrier
        false,      // LSB first
        repeats > 0 ? repeats : 1,
        33          // 33% duty cycle
    );
    return true;
}

bool IREngine::decodeRCAFromRaw(const uint16_t* raw_arr, size_t len, uint32_t& out_address, uint32_t& out_command) {
    if (!raw_arr || len < 50) return false;

    // Preamble check (~4000us mark, ~4000us space)
    if (raw_arr[0] < 3000 || raw_arr[0] > 5000) return false;
    if (raw_arr[1] < 3000 || raw_arr[1] > 5000) return false;

    uint32_t data = 0;
    for (size_t i = 0; i < 24; i++) {
        size_t mark_idx = 2 + 2 * i;
        size_t space_idx = mark_idx + 1;
        if (space_idx >= len) return false;

        // Mark should be ~500us
        if (raw_arr[mark_idx] < 250 || raw_arr[mark_idx] > 850) return false;

        // Space: Bit 0 is ~1000us, Bit 1 is ~2000us
        uint16_t sp = raw_arr[space_idx];
        if (sp >= 1500 && sp <= 2600) {
            data |= (1UL << i); // LSB first
        } else if (sp >= 600 && sp < 1500) {
            // bit 0
        } else {
            return false; // Timing violation
        }
    }

    uint8_t addr = data & 0x0F;
    uint8_t cmd = (data >> 4) & 0xFF;
    uint8_t inv_addr = (data >> 12) & 0x0F;
    uint8_t inv_cmd = (data >> 16) & 0xFF;

    if (addr == ((~inv_addr) & 0x0F) && cmd == ((~inv_cmd) & 0xFF)) {
        out_address = addr;
        out_command = cmd;
        return true;
    }
    return false;
}

bool IREngine::sendParsed(const String& protocol, uint32_t address, uint32_t command, uint16_t nbits) {
    String p = protocol;
    p.trim();

    if (p.equalsIgnoreCase("RCA")) {
        return sendRCA(address, command, 1);
    }

    decode_type_t type = strToDecodeType(protocol);
    if (type == DECODE_TYPE_RCA) {
        return sendRCA(address, command, 1);
    }

    if (type == NEC) {
        uint64_t data;
        // If address is non-zero, or command is small (<= 0xFF, e.g. from Flipper .ir file)
        if (address != 0 || command <= 0xFF) {
            data = irsend.encodeNEC(address, command);
        } else {
            // command is already the full 32-bit NEC frame (e.g. from captured results.value)
            data = command;
        }
        irsend.sendNEC(data, nbits > 0 ? nbits : 32, 1);
        return true;
    } else if (type == SAMSUNG) {
        uint64_t data;
        if (address != 0 && command <= 0xFFFF) {
            data = irsend.encodeSAMSUNG(address, command);
        } else {
            data = command;
        }
        irsend.sendSAMSUNG(data, nbits > 0 ? nbits : 32, 1);
        return true;
    } else if (type == SONY) {
        uint16_t bits = (p.equalsIgnoreCase("SIRC15")) ? 15 : ((p.equalsIgnoreCase("SIRC20")) ? 20 : (nbits > 0 ? nbits : 12));
        uint64_t data;
        if (address != 0) {
            data = irsend.encodeSony(bits, command, address);
        } else {
            data = command;
        }
        irsend.sendSony(data, bits, 2); // Sony SIRC requires min 2 repeats
        return true;
    } else if (type == RC5) {
        uint64_t data;
        if (address != 0) {
            data = (p.equalsIgnoreCase("RC5X")) ? irsend.encodeRC5X(address, command) : irsend.encodeRC5(address, command);
        } else {
            data = command;
        }
        irsend.sendRC5(data, nbits > 0 ? nbits : 12, 1);
        return true;
    } else if (type == RC6) {
        uint64_t data;
        if (address != 0) {
            data = irsend.encodeRC6(address, command);
        } else {
            data = command;
        }
        irsend.sendRC6(data, nbits > 0 ? nbits : 20, 1);
        return true;
    } else if (type == NIKAI) {
        uint16_t bits = (nbits > 0) ? nbits : 24;
        irsend.sendNikai(command, bits, 1); // 1 repeat for Nikai
        return true;
    } else if (type != UNKNOWN) {
        uint16_t bits = (nbits > 0) ? nbits : IRsend::defaultBits(type);
        uint16_t repeats = std::max((uint16_t)1, IRsend::minRepeats(type));
        irsend.send(type, command, bits, repeats);
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

    bool success = false;
    if (btn.type == IrSignalType::PARSED) {
        success = sendParsed(btn.protocol, btn.address, btn.command, btn.nbits);
        if (!success && !btn.raw_data.empty()) {
            success = sendRaw(btn.raw_data.data(), btn.raw_data.size(), btn.frequency);
        }
    } else {
        if (!btn.raw_data.empty()) {
            success = sendRaw(btn.raw_data.data(), btn.raw_data.size(), btn.frequency);
        }
    }
    return success;
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
            // Check if captured raw signal matches RCA protocol
            uint32_t rca_addr = 0, rca_cmd = 0;
            if (decodeRCAFromRaw(out_btn.raw_data.data(), out_btn.raw_data.size(), rca_addr, rca_cmd)) {
                out_btn.type = IrSignalType::PARSED;
                out_btn.protocol = "RCA";
                out_btn.address = rca_addr;
                out_btn.command = rca_cmd;
                out_btn.nbits = 24;
                out_btn.frequency = 38000;
                out_btn.duty_cycle = 0.33f;
                out_btn.has_duty_cycle = true;
            } else {
                out_btn.type = IrSignalType::RAW;
                out_btn.protocol = "RAW";
                out_btn.address = 0;
                out_btn.command = 0;
                out_btn.nbits = 0;
                out_btn.frequency = 0; // 0 = unknown carrier
                out_btn.duty_cycle = 0.0f;
                out_btn.has_duty_cycle = false;
            }
        }

        irrecv.resume();
        return true;
    }
    return false;
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
                if (current_btn.nbits == 0) current_btn.nbits = getProtocolDefaultBits(current_btn.protocol);
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
        } else if (key == "nbits") {
            current_btn.nbits = val.toInt();
        } else if (key == "frequency") {
            current_btn.frequency = val.toInt();
        } else if (key == "duty_cycle") {
            current_btn.duty_cycle = val.toFloat();
            current_btn.has_duty_cycle = true;
        } else if (key == "data") {
            current_btn.raw_data.clear();
            const char* p = val.c_str();
            while (*p != '\0') {
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '\0') break;
                char* endp = nullptr;
                unsigned long num = strtoul(p, &endp, 10);
                if (endp == p) break;
                if (current_btn.raw_data.size() < MAX_IR_RAW_TIMINGS) {
                    current_btn.raw_data.push_back((uint16_t)num);
                } else {
                    current_btn.truncated = true;
                }
                p = endp;
            }
        }
    }

    if (in_button) {
        if (current_btn.nbits == 0) current_btn.nbits = getProtocolDefaultBits(current_btn.protocol);
        remote.buttons.push_back(current_btn);
    }

    return !remote.buttons.empty();
}

bool IREngine::saveIrFile(const String& path, const IrRemoteFile& remote) {
    // Pre-calculate approximate size to avoid frequent reallocations
    size_t approx_size = 64;
    for (size_t i = 0; i < remote.buttons.size(); i++) {
        approx_size += 128 + remote.buttons[i].raw_data.size() * 7;
    }
    String out;
    out.reserve(approx_size);
    out = "Filetype: IR library file\nVersion: 1\n#\n";

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
            out += "command: " + String(hexBuf) + "\n";
            if (b.nbits > 0) {
                out += "nbits: " + String(b.nbits) + "\n";
            }
            if (!b.raw_data.empty()) {
                out += "data:";
                size_t write_count = min(b.raw_data.size(), MAX_IR_RAW_TIMINGS);
                char numBuf[16];
                for (size_t j = 0; j < write_count; j++) {
                    snprintf(numBuf, sizeof(numBuf), " %u", b.raw_data[j]);
                    out += numBuf;
                }
                out += "\n";
            }
            out += "#\n";
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
            char numBuf[16];
            for (size_t j = 0; j < write_count; j++) {
                snprintf(numBuf, sizeof(numBuf), " %u", b.raw_data[j]);
                out += numBuf;
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

    // Check /ir directory
    if (fileManager.exists("/ir")) {
        size_t count = fileManager.listDir("/ir", entries, 32);
        for (size_t i = 0; i < count; i++) {
            if (!entries[i].isDir && entries[i].name.endsWith(".ir")) {
                files.push_back("/ir/" + entries[i].name);
            }
        }
    }

    // Also check root / directory
    size_t count_root = fileManager.listDir("/", entries, 32);
    for (size_t i = 0; i < count_root; i++) {
        if (!entries[i].isDir && entries[i].name.endsWith(".ir")) {
            String path = "/" + entries[i].name;
            bool exists_in_list = false;
            for (const auto& f : files) {
                if (f == path) { exists_in_list = true; break; }
            }
            if (!exists_in_list) files.push_back(path);
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
                if (code.type == DECODE_TYPE_RCA) {
                    uint32_t addr = (code.data >> 8) & 0x0F;
                    uint32_t cmd = code.data & 0xFF;
                    sendRCA(addr, cmd, 1);
                } else if (code.type == NIKAI) {
                    irsend.sendNikai(code.data, code.nbits > 0 ? code.nbits : 24, 1);
                } else if (code.type == SONY) {
                    irsend.sendSony(code.data, code.nbits > 0 ? code.nbits : 12, 2);
                } else if (code.type == NEC) {
                    irsend.sendNEC(code.data, code.nbits > 0 ? code.nbits : 32, 1);
                } else {
                    irsend.send(code.type, code.data, code.nbits);
                }
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
    digitalWrite(IR_TX, polarity_inverted ? HIGH : LOW);
    irsend.begin();
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32)
    gpio_set_drive_capability((gpio_num_t)IR_TX, GPIO_DRIVE_CAP_3);
#endif
}

void IREngine::pulseLedDc(uint32_t duration_ms) {
    if (carrier_test_active) stopCarrierTest();
    pinMode(IR_TX, OUTPUT);
    digitalWrite(IR_TX, polarity_inverted ? LOW : HIGH); // turn LED fully on
    delay(duration_ms);
    digitalWrite(IR_TX, polarity_inverted ? HIGH : LOW); // turn LED off
    irsend.begin();
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32)
    gpio_set_drive_capability((gpio_num_t)IR_TX, GPIO_DRIVE_CAP_3);
#endif
}

bool IREngine::runLoopbackTest(String& out_result) {
    if (carrier_test_active) stopCarrierTest();
    irrecv.enableIRIn();
    delay(30);

    // Send a known test code: NEC 0x00FF807F (Address 0x00FF, Command 0x807F)
    const uint32_t TEST_CODE = 0x00FF807F;
    irsend.sendNEC(TEST_CODE, 32, 0);

    uint32_t start = millis();
    bool received = false;
    decode_results res;
    while (millis() - start < 350) {
        if (irrecv.decode(&res)) {
            received = true;
            break;
        }
        delay(10);
    }
    irrecv.disableIRIn();
    if (capturing) irrecv.enableIRIn();

    if (received) {
        char hexb[16];
        snprintf(hexb, sizeof(hexb), "%08X", (uint32_t)res.value);
        out_result = "PASS " + typeToString(res.decode_type) + " " + String(hexb);
        return true;
    } else {
        out_result = "FAIL NO RX DETECT";
        return false;
    }
}
