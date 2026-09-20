# Bolt's Performance Journal ⚡

## 2026-03-29 - ESP32-S3 Single-Precision FPU Math Acceleration
**Learning:** Default C/C++ math calls like `sqrt()`, `atan2()`, `asin()`, `sin()`, and `cos()` take `double` or convert `float` arguments to 64-bit `double` precision. The ESP32-S3 (Xtensa LX7 dual-core) hardware FPU only supports single-precision (32-bit `float`) hardware instructions. Calling double-precision math variants forces the CPU into costly software double-precision emulation.
**Action:** Always use single-precision math function variants (`sqrtf()`, `atan2f()`, `asinf()`, `sinf()`, `cosf()`) in high-frequency loops (such as Madgwick sensor fusion loops or frame-rendering math) when working with `float` types on ESP32/ESP32-S3 targets.
