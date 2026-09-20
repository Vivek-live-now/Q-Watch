# Bolt's Performance Journal ⚡

## 2026-03-29 - ESP32-S3 Single-Precision FPU Math Acceleration
**Learning:** Default C/C++ math function calls like `sqrt()`, `atan2()`, `asin()`, `sin()`, and `cos()` operate on `double` or convert `float` arguments to 64-bit `double` precision. The ESP32-S3 (Xtensa LX7 dual-core) hardware FPU is specifically designed and optimized for single-precision (32-bit `float`) hardware instructions. Using the `f`-suffixed variants (`sqrtf()`, `atan2f()`, `asinf()`, `sinf()`, `cosf()`) and `float` literals (e.g. `180.0f`) avoids unnecessary double-precision computation and type conversion overhead when processing float dataset pipelines.
**Action:** Always prefer single-precision math function variants (`sqrtf()`, `atan2f()`, `asinf()`, `sinf()`, `cosf()`) and explicit `float` literals (e.g., `180.0f`) in high-frequency loops (such as Madgwick sensor fusion loops or frame-rendering math) when working with `float` types on ESP32/ESP32-S3 targets.
