with open('src/ir_engine.cpp', 'r') as f:
    content = f.read()

old_load = '''void IREngine::loadDefaultTvBGoneCodes() {
    tv_bgone_codes.clear();
    for (size_t i = 0; i < DEFAULT_TV_POWER_CODES_COUNT; i++) {
        tv_bgone_codes.push_back(DEFAULT_TV_POWER_CODES[i]);
    }
    tv_bgone_total = tv_bgone_codes.size();
}'''

new_load = '''void IREngine::loadDefaultTvBGoneCodes() {
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
}'''

if old_load in content:
    content = content.replace(old_load, new_load)
    with open('src/ir_engine.cpp', 'w') as f:
        f.write(content)
    print("Updated loadDefaultTvBGoneCodes successfully.")
else:
    print("Could not find old_load.")
