# Q-Watch V1 Firmware Storage & Cleanup Forensic Audit

## 1. Audit scope

Audit target: `Vivek-live-now/Q-Watch`, branch state based on `main` at merge commit `6089cdc0049ef19254d923b46947b60c3b69db99`.

This is an analysis-only engineering audit. No firmware feature is proposed for removal. The objective is to identify unnecessary implementation cost, duplication, source/tooling clutter, and safe architectural opportunities that could reduce flash/RAM pressure while preserving Q-Watch V1 behavior.

Important distinction:

- Repository/source size is not firmware flash size.
- LittleFS contents are not application flash usage.
- RAM usage is separate from flash usage.
- Removing source lines does not necessarily reduce the final binary.
- A dependency can be large in source terms but have only a small linked footprint if dead code elimination removes unused sections.
- Conversely, a small source change can pull a large library subsystem into the final image.

---

## 2. Current measured firmware baseline

The latest successful CI build for the post-IR main commit reports:

- RAM: 83,152 / 327,680 bytes = 25.4%
- Application flash: 2,253,773 / 3,145,728 bytes = 71.6%
- Remaining application space: 891,955 bytes

The previous successful main build immediately before the IR merge reports:

- RAM: 82,088 / 327,680 bytes = 25.1%
- Application flash: 2,145,645 / 3,145,728 bytes = 68.2%
- Remaining application space: 1,000,083 bytes

Therefore the change set merged as PR #21 corresponds to an observed increase of:

- +108,128 bytes application flash
- +1,064 bytes RAM

This is a useful regression baseline. It does NOT prove that all +108,128 bytes are IR engine code because the change also modifies UI/display code and adds the IRremoteESP8266 dependency. Attribution requires section/symbol inspection.

---

## 3. Repository structure and firmware contributors

The principal firmware source files are:

| File | Approx. source size | Initial assessment |
|---|---:|---|
| src/display.cpp | 62.5 KB | Major flash investigation target |
| src/ui_core.cpp | 59.3 KB | Major flash investigation target |
| src/wifi_portal.cpp | 27.3 KB | Major flash/rodata investigation target |
| src/sensors.cpp | 26.2 KB | Major logic target, likely legitimate complexity |
| src/ir_engine.cpp | 18.5 KB | Major post-PR investigation target |
| src/max30102_manager.cpp | 9.0 KB | Moderate |
| src/air_mouse.cpp | 8.8 KB | Moderate, BLE framework cost likely more important |
| src/button_manager.cpp | 6.3 KB | Small/moderate |
| src/keyboard.cpp | 6.1 KB | Small/moderate |
| remaining modules | <6 KB each | Lower priority |

The four largest source files alone account for roughly 175 KB of C++ source text, but that number must not be treated as firmware flash consumption.

---

## 4. Flash versus RAM diagnosis

The immediate pressure is application flash, not RAM.

At 71.6% application flash, the project is not yet at the partition limit, but the post-IR jump of approximately 108 KB is large enough that future feature additions could become constrained.

RAM at 25.4% is comparatively comfortable.

Cleanup should therefore prioritize:

1. linked .text
2. linked .rodata
3. library-linked code
4. duplicated protocol/UI/data implementations
5. only then RAM optimization

Large arrays and buffers should still be reviewed because RAM headroom matters for future features, but they are not currently the primary constraint.

---

## 5. PR #21 storage impact

The merge-base comparison for PR #21 shows:

- 3,711 additions
- 451 deletions
- 18 changed files
- new IR engine implementation
- new IR header
- IRremoteESP8266 dependency
- substantial UI/display additions
- several Python generation/test scripts

The firmware grew by approximately 108 KB in the CI build.

This makes PR #21 the first cleanup hotspot.

The correct question is not “remove IR.” The correct questions are:

- Which IR functionality is actually linked?
- Which protocol support is pulled in by IRremoteESP8266?
- Which functionality is duplicated between Q-Watch code and the library?
- Which UI code is duplicated?
- Which protocol conversion code is necessary for compatibility?
- Which test/database/generator material is accidentally compiled?
- Can the same V1 functionality be retained with a smaller linked implementation?

---

## 6. IR engine audit

### High-priority investigation

`src/ir_engine.cpp` is about 18.5 KB of source and introduces a broad abstraction around:

- parsed IR
- raw IR
- Flipper-compatible file parsing
- transmission
- learning
- raw-to-parsed analysis
- TV-B-GONE
- recent/favorites
- carrier testing

The architecture is coherent enough to keep, but several areas deserve measurement.

### Potential duplicated responsibility

IRremoteESP8266 already contains substantial protocol decoding and encoding infrastructure. Q-Watch adds protocol translation and file-format handling around it.

This wrapper is justified for V1 because Q-Watch needs a common remote/file/UI abstraction. However, every custom encoder, decoder adapter, protocol-name mapper, and conversion path should be proven necessary.

### Important correctness finding

`saveIrFile()` currently emits:

`Filetype: IR library file`

rather than the generic Flipper signal-file header:

`Filetype: IR signals file`

That should be treated as a compatibility correctness investigation, not as a cleanup opportunity.

Do not change it blindly. First verify which files Q-Watch intends to support and whether the current output is accepted by Flipper/Bruce.

### Protocol fidelity investigation

The internal representation collapses several protocol variants into broader IRremote types. For example, NEC/NECext/NEC42 are mapped through the same `decode_type_t` path.

This may be safe for some transmissions but can lose semantic fidelity for interoperability.

Do not simplify these mappings until hardware and file round-trip tests prove equivalence.

---

## 7. TV-B-GONE database

The compiled default table currently contains 14 codes.

This is a small amount of source data compared with the overall binary, but the architecture is more interesting than the table itself.

The engine also supports loading additional codes from `/ir/tvbgone.ir`.

This means future expansion does not necessarily need to become compiled firmware data.

### Candidate cleanup direction

Keep a small built-in fallback set, while considering whether the complete optional TV-B-GONE database can live in LittleFS.

This preserves functionality if the fallback is retained, while moving bulk data out of application flash.

Do not claim a saving until the actual table/data representation and resulting linked sections are measured.

---

## 8. Raw IR storage

`MAX_IR_RAW_TIMINGS = 1024` is intentional and aligns with the desired interoperability target.

Do not reduce it to 512 merely to save RAM or flash.

The vector allocation is dynamic, so the constant itself is not a 1024-element static RAM allocation.

Potential future optimization:

- avoid repeated temporary String construction while parsing long raw records
- reserve vector capacity when appropriate
- stream very large raw files instead of reading the entire file into a String

These are primarily RAM/latency optimizations and should not be assumed to reduce firmware flash.

---

## 9. IR parser memory behavior

`parseIrFile()` currently reads the entire file into a String.

For ordinary remote files this is convenient and probably sufficient.

For large raw signals it can temporarily hold:

- complete file text
- String substrings
- vector elements
- parser temporaries

This is a RAM concern rather than the primary flash concern.

A future stream parser could reduce peak RAM, but it would add code complexity. Therefore this belongs in “investigate,” not immediate cleanup.

---

## 10. IR raw-to-parsed conversion

`analyzeRawToParsed()` manually constructs a `decode_results` buffer and feeds it into IRremoteESP8266.

This is a high-risk area for simplification.

The code is attempting to reuse the library decoder rather than implementing its own protocol decoder, which is architecturally sensible.

However, hardware validation is required because `IRrecv::decode()` normally operates on receiver-generated state.

Recommendation:

- retain until validated
- create a deterministic protocol test matrix
- compare raw capture -> parsed -> transmit -> receiver decode
- only simplify after equivalence is demonstrated

---

## 11. UI source footprint

`src/display.cpp` and `src/ui_core.cpp` are each around 60 KB of source.

They contain a large number of feature-specific screens and state handlers.

Because V1 intentionally has many applications, splitting or shortening these files alone will not necessarily reduce flash.

The strongest cleanup opportunity is repeated behavior.

Look specifically for repeated:

- menu rendering
- selection/scroll calculations
- value rendering
- status/footer drawing
- page navigation
- repeated String construction
- repeated toast/status messages
- identical button-event handling patterns

A generic helper can reduce duplicated machine code only if the compiler does not already fold equivalent code.

Measure the ELF before and after any proposed refactor.

---

## 12. Display-specific findings

There are four repeated:

`#include "keyboard.h"`

lines in `src/display.cpp`.

This is definite source cleanup.

Because `keyboard.h` has include guards, the duplicate includes should not materially increase final firmware size. Therefore this is a cleanliness finding, not a flash-saving finding.

There is also a visible `[NOT IMPLEMENTED]` reset-settings screen.

That is not a cleanup candidate. It is a functional completeness issue and should be tracked separately.

The weather home renderer also contains fixed display values in the inspected implementation. That should be verified against the intended weather architecture before any refactor. Do not optimize or delete it merely because it looks old.

---

## 13. Wi-Fi and Web File Manager

`src/wifi_portal.cpp` is approximately 27.3 KB.

String literals account for roughly 12.4 KB of source literal text in this file, making it a strong candidate for .rodata investigation.

The embedded web UI is a particularly attractive architectural candidate.

### Possible no-downgrade alternative

Move large static HTML/CSS/JavaScript resources into LittleFS and have the existing WebServer serve them.

Keep the API endpoints in firmware.

Benefits:

- reduces application .rodata
- preserves Web File Manager functionality
- makes web UI easier to edit
- uses already-existing LittleFS infrastructure

Risks:

- boot/storage assumptions
- missing files after format
- filesystem versioning
- serving resources before LittleFS is mounted
- increased request complexity

This should be prototyped only after measuring the current .rodata contribution.

---

## 14. Generated Python scripts

The repository contains several Python scripts used to generate/update source files.

Notably:

- `update_display_ir.py`
- `update_ui_core.py`
- `update_anal.py`
- `update_qr.py`
- `update_samples.py`
- `update_tvbg.py`
- `update_readme.py`
- `update_h.py`

These do not directly contribute to the ESP32 firmware binary because they are Python tooling.

However, they create repository maintenance risk.

The largest are:

- `update_ui_core.py`: about 58.6 KB
- `update_display_ir.py`: about 10.4 KB

These should be classified as repository/tooling cleanup, not firmware cleanup.

Before deleting any, verify whether Jules or another workflow still depends on them.

---

## 15. Dependency audit

Current direct PlatformIO dependencies:

- U8g2
- ArduinoJson
- FastLED
- Adafruit BME280
- Adafruit Unified Sensor
- SparkFun MAX3010x
- IRremoteESP8266

All should be treated as intentional until actual include/link evidence says otherwise.

Particular attention:

### IRremoteESP8266

New with PR #21 and therefore the strongest flash-growth suspect.

Do not replace it automatically. Its value is substantial because it provides protocol decoding/encoding.

The correct experiment is to obtain symbol/section data and identify exactly what the linker pulls in.

### ArduinoJson

Used by the weather/network stack if current source confirms it. Verify linked contribution before considering alternatives.

### Adafruit Unified Sensor

Check whether it is directly referenced or only required transitively by Adafruit BME280. If only transitively required, removing it from `lib_deps` may have no effect or may break dependency resolution. Do not change until tested.

### FastLED

Provides the RGB LED feature. Keep functionality intact. Measure whether the current one-pixel use pulls in more functionality than necessary.

---

## 16. BLE Air Mouse

Air Mouse is an intentional V1 feature.

The source file is only about 8.8 KB, but BLE framework code can be substantially larger than the feature's own source.

Therefore the source file size is misleading.

Potential investigation:

- identify which BLE stack components are actually linked
- determine whether BLE HID can be isolated into a smaller build configuration
- check whether unused BLE profiles/services are pulled in

Do not remove BLE or Air Mouse.

---

## 17. Sensors and numerical code

`src/sensors.cpp` is approximately 26.2 KB.

Its complexity is justified by:

- MPU6500
- QMC5883P
- calibration
- Madgwick fusion
- orientation
- deep-sleep motion interrupt
- BME280
- history logging
- altitude/reference pressure

This should not be aggressively compressed.

Potential safe cleanup:

- remove duplicated calculations only after verifying numerical equivalence
- centralize repeated calibration persistence logic
- ensure constants are typed efficiently
- verify unused calibration fields/functions through call-graph analysis

Do not sacrifice numerical stability or sensor behavior for line-count reduction.

---

## 18. RAM hotspots

Known substantial RAM consumers include:

- MAX30102 sample arrays:
  - 100 red samples
  - 100 IR samples
- BME history buffer:
  - 288 entries
- display magnetic history:
  - 64 floats
- IR raw vectors
- Wi-Fi scan network array
- UI File Manager allocation
- BLE objects when Air Mouse is active

Current RAM usage is only 25.4%, so none should be removed solely for pressure.

The BME history and MAX30102 sample buffers are feature requirements and should remain unless an equivalent storage/processing design is proven.

---

## 19. File Manager and LittleFS architecture

The FileManager wrapper is relatively small, approximately 4 KB of source.

Its API is simple and provides a useful abstraction boundary.

However, some functions are thin one-line wrappers around LittleFS.

Removing the abstraction would reduce source lines but could increase coupling throughout the firmware and would not necessarily reduce final flash.

Recommendation: keep the abstraction.

A better future cleanup is to ensure all filesystem consumers consistently use FileManager rather than mixing direct LittleFS access and FileManager access without a reason.

---

## 20. Source-level cleanup candidates

### High confidence, low risk

1. Remove repeated `#include "keyboard.h"` lines in `src/display.cpp`.
2. Remove obsolete comments that describe already-removed implementations.
3. Verify and remove genuinely unreachable helper functions after call-graph analysis.
4. Consolidate duplicated constants only where linkage and behavior remain identical.
5. Keep tooling scripts separate from firmware architecture.

### Expected flash saving

The first item is effectively zero.

The others require ELF measurement.

---

## 21. Investigate-before-changing candidates

### A. Embedded web UI

Potentially meaningful .rodata reduction by moving static assets to LittleFS.

### B. IRremoteESP8266 linkage

Potentially the largest single library-level optimization target.

### C. IR protocol adapters

Measure whether custom protocol conversion code duplicates linked library functionality.

### D. UI repeated rendering

Potentially meaningful .text reduction if repeated functions are actually emitted separately.

### E. BLE framework

Potentially large, but feature-critical.

### F. TV-B-GONE data

Move optional database data to LittleFS if not already effectively externalized.

### G. Recent/Favorites representation

Current persistence is small and should not be optimized aggressively.

---

## 22. Do-not-touch list

Do not remove or downgrade:

- IR functionality
- Flipper/Bruce interoperability
- 1024-timing raw capability
- Quick Remote
- TV-B-GONE
- Universal IR architecture
- BLE Air Mouse
- BME history
- MAX30102 history
- MPU6500 interrupt/deep sleep
- QMC5883P fusion/calibration
- File Manager
- Web File Manager
- Virtual Keyboard
- Wi-Fi scanning
- NTP
- Weather
- RGB/LED manager
- Sound manager
- settings persistence
- LittleFS
- custom ESP32-S3 SuperMini board definition

Do not reduce functionality merely to obtain a lower flash percentage.

---

## 23. Recommended cleanup sequence

### Phase 1: Measurement

Before modifying firmware:

1. Capture ELF section sizes.
2. Capture top symbols by flash contribution.
3. Capture library archive contributions.
4. Record RAM sections.
5. Record baseline after PR #21.

### Phase 2: Zero-risk source cleanup

Only source-level cleanliness changes with no behavioral effect.

Examples:

- duplicate includes
- obsolete comments
- unreachable development scaffolding confirmed by call graph

### Phase 3: Largest measured contributors

Investigate in this order:

1. IRremoteESP8266 linked footprint
2. Web UI .rodata
3. UI/display repeated code
4. BLE framework footprint
5. other library contributions

### Phase 4: Data relocation

Move only appropriate static data to LittleFS.

Candidates:

- web assets
- optional IR databases
- large configurable tables

Do not move data required for boot-critical functionality unless a safe fallback exists.

### Phase 5: Validation

For every cleanup:

- build
- compare flash
- compare RAM
- run CI
- hardware-test affected subsystem
- verify no behavior regression
- document measured savings

### Final engineering principle

The objective is not “make the source smaller.”

The objective is:

**Preserve the complete Q-Watch V1 feature set while reducing unnecessary linked firmware footprint.**

Every proposed change should therefore answer four questions:

1. What code/data is currently costing space?
2. Why is it present?
3. What exact functionality does it support?
4. Can the same functionality be retained with less linked code?

If those questions cannot be answered confidently, leave the code alone until better measurements exist.
