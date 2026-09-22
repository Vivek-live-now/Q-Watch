with open('src/ir_engine.cpp', 'r') as f:
    content = f.read()

old_anal = '''bool IREngine::analyzeRawToParsed(IrButton& btn) {
    if (btn.type != IrSignalType::RAW || btn.raw_data.empty()) return false;

    // Use real IRrecv decoder rather than hardcoded fake data
    // Returns false if raw signal cannot be decoded to a real protocol
    return false;
}'''

new_anal = '''bool IREngine::analyzeRawToParsed(IrButton& btn) {
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
}'''

if old_anal in content:
    content = content.replace(old_anal, new_anal)
    with open('src/ir_engine.cpp', 'w') as f:
        f.write(content)
    print("Updated analyzeRawToParsed successfully.")
else:
    print("Could not find old_anal.")
