with open('README.md', 'r') as f:
    content = f.read()

milestone_ir = '''
### Milestone 8: Full IR Remote Subsystem & Bruce / Flipper Zero Compatibility
* **Pure Menu Architecture:** Structured as a 9-item scrolling menu shell (`TV-B-GONE`, `CUSTOM IR`, `IR READ`, `QUICK REMOTE`, `UNIVERSAL`, `RECENT`, `FAVORITES`, `IR FILES`, `IR LAB`) on the 128x64 OLED display.
* **Flipper Zero & Bruce `.ir` File Compatibility:** Dedicated parser and serializer for standard `.ir` library files, supporting both `type: parsed` (protocol, address, command, nbits) and `type: raw` (frequency, duty_cycle, microsecond pulse data). Files stored under `/ir/` on LittleFS can be transferred seamlessly between Bruce, Flipper Zero, and Q-Watch.
* **Exact Protocol Variant Preservation:** Full support and 100% round-trip fidelity for `NEC`, `NECext`, `NEC42`, `Samsung32`, `RC5`, `RC5X`, `RC6`, `SIRC` (Sony 12-bit), `SIRC15`, and `SIRC20`. Preserves exact multi-byte hex addresses/commands, bit counts, and raw carrier frequency/duty cycle attributes without inventing unmeasured default values.
* **Non-Blocking IR Read & RX Lifecycle:** Asynchronous signal capture pipeline (`IRrecv` on GPIO 17) with explicit receiver ownership. RX automatically pauses during transmissions (`IRsend` on GPIO 18) and when navigating away from capture screens.
* **Interactive Quick Remote Cloning:** Multi-button remote builder workflow: Name Remote → Name Button → Learn Signal → Test Transmission → Save Button → Add Additional Buttons → Save as standard `.ir` file.
* **TV-B-GONE Power Blaster:** Non-blocking power code transmitter featuring animated progress status (`CODE X/Y`), immediate cancellation on Short CANCEL, and data-driven code expansion support via `/ir/tvbgone.ir` on LittleFS.
* **IR Signal Lab Diagnostics:** Engineering laboratory tool featuring 36 kHz, 38 kHz, and 40 kHz PWM carrier output validation (LEDC channel) and real-time raw-to-parsed protocol decoding analysis.
* **File Manager Integration:** Selecting any `.ir` file directly from the main Q-Watch File Manager (`APP_FILE_MANAGER`) launches the IR Remote viewer to inspect and transmit buttons immediately.
* **Signal Limits & Storage:** Strict `MAX_IR_RAW_TIMINGS = 1024` buffer enforcement across parsing, capturing, and serialization. Persistent Recent signals log (`/ir/recent.txt`) and Favorite button shortcuts (`/ir/favorites.txt`).
'''

target_marker = "### Milestone 7: IMU6500 Sub-App Architecture & BLE Air Mouse Mode"

if target_marker in content:
    idx = content.find("## Hardware Architecture & Pinout")
    if idx >= 0:
        content = content[:idx] + milestone_ir + "\n" + content[idx:]
        with open('README.md', 'w') as f:
            f.write(content)
        print("Updated README.md with Milestone 8.")
    else:
        print("Could not find hardware section index.")
else:
    print("Could not find Milestone 7 marker.")
