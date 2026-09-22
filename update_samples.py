with open('src/ir_engine.cpp', 'r') as f:
    content = f.read()

old_ensure = '''void IREngine::ensureIrDirectory() {
    if (!fileManager.exists("/ir")) {
        fileManager.create("/ir/.keep");
    }
    if (!fileManager.exists("/ir/universal")) {
        fileManager.create("/ir/universal/.keep");
    }
}'''

new_ensure = '''void IREngine::ensureIrDirectory() {
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
}'''

if old_ensure in content:
    content = content.replace(old_ensure, new_ensure)
    with open('src/ir_engine.cpp', 'w') as f:
        f.write(content)
    print("Updated ensureIrDirectory with sample files.")
else:
    print("Could not find ensureIrDirectory.")
