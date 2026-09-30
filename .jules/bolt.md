# Bolt's Performance Journal ⚡

## 2026-03-29 - ESP32-S3 Single-Precision FPU Math Acceleration
**Learning:** Default C/C++ math function calls like `sqrt()`, `atan2()`, `asin()`, `sin()`, and `cos()` operate on `double` or convert `float` arguments to 64-bit `double` precision. The ESP32-S3 (Xtensa LX7 dual-core) hardware FPU is specifically designed and optimized for single-precision (32-bit `float`) hardware instructions. Using the `f`-suffixed variants (`sqrtf()`, `atan2f()`, `asinf()`, `sinf()`, `cosf()`) and `float` literals (e.g. `180.0f`) avoids unnecessary double-precision computation and type conversion overhead when processing float dataset pipelines.
**Action:** Always prefer single-precision math function variants (`sqrtf()`, `atan2f()`, `asinf()`, `sinf()`, `cosf()`) and explicit `float` literals (e.g., `180.0f`) in high-frequency loops (such as Madgwick sensor fusion loops or frame-rendering math) when working with `float` types on ESP32/ESP32-S3 targets.

## 2026-09-29 - Xtensa LX7 PIE Vector Instructions & PROGMEM Asset Relocation
**Learning:**
1. The ESP32-S3's Xtensa LX7 core features the Processor Interface Extension (PIE) coprocessor with 128-bit vector registers (`q0`-`q7`). For fast 128-bit vector bitwise operations, the canonical Xtensa LX7 TRM opcodes are `ee.notq`, `ee.xorq`, `ee.andq`, and `ee.orq`. Operating across 128-bit chunks accelerates framebuffer transforms (1024-byte invert/blit) by ~4x compared to 32-bit scalar loops.
2. Embedding large web portal HTML/CSS/JS/SVG buffers as plain C strings consumes vital internal DRAM heap memory. Migrating them to `const char PROGMEM` stores them in flash memory, freeing >14 KB of heap for TLS handshakes, FreeRTOS task stacks, and relocatable `.qapp` binaries.
3. High-frequency time-series logging (BME280 / MAX30102) with single-record file truncations induces severe SPI flash erase-block wear. Buffering with a 32-entry hysteresis threshold and `readSeek` tail recovery cuts flash erase cycles by ~97%.
**Action:** Use Xtensa LX7 PIE vector instructions for aligned 128-bit operations with scalar fallback, migrate static web assets to `PROGMEM`, and enforce hysteresis thresholds on circular log pruners.
