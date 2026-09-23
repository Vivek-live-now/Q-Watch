#ifndef QWATCH_API_H
#define QWATCH_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// -------------------------------------------------------------
// 1. MAGIC & VERSIONING
// -------------------------------------------------------------
#define QAPP_MAGIC 0x51415050U // ASCII "QAPP"
#define QAPP_API_VERSION 1U
#define QAPP_MIN_COMPAT_VERSION 1U

// -------------------------------------------------------------
// 2. ERROR CODES
// -------------------------------------------------------------
typedef enum {
    QAPP_OK                     = 0,
    QAPP_ERR_MAGIC              = -1,
    QAPP_ERR_VERSION            = -2,
    QAPP_ERR_CAPS_UNSUPPORTED   = -3,
    QAPP_ERR_OOM                = -4,
    QAPP_ERR_FILE_NOT_FOUND     = -5,
    QAPP_ERR_INIT_FAILED        = -6,
    QAPP_ERR_CORRUPT_HEADER     = -7,
    QAPP_ERR_ALREADY_RUNNING    = -8,
    QAPP_ERR_INVALID_PARAM      = -9,
    QAPP_ERR_NULL_ENTRY         = -10
} QAppErrorCode;

// -------------------------------------------------------------
// 3. HARDWARE CAPABILITY FLAGS
// -------------------------------------------------------------
#define QAPP_CAP_NONE        0U
#define QAPP_CAP_DISPLAY     (1U << 0)  // SH1106 128x64 OLED
#define QAPP_CAP_BUTTONS     (1U << 1)  // 4-Button Navigation (UP, OK, DN, CANCEL)
#define QAPP_CAP_MPU         (1U << 2)  // MPU-6500 6-Axis Motion & Madgwick Fusion
#define QAPP_CAP_MAG         (1U << 3)  // QMC5883P 3D Magnetometer / Compass
#define QAPP_CAP_HEALTH      (1U << 4)  // MAX30102 Pulse Oximeter & PPG
#define QAPP_CAP_BME         (1U << 5)  // BME280 Environmental Sensor
#define QAPP_CAP_AUDIO       (1U << 6)  // Buzzer Audio / Tones
#define QAPP_CAP_RGB_LED     (1U << 7)  // WS2812 Onboard RGB LED
#define QAPP_CAP_IR          (1U << 8)  // Infrared Transceiver (GPIO 18 TX, 17 RX)
#define QAPP_CAP_STORAGE     (1U << 9)  // LittleFS App Sandbox
#define QAPP_CAP_ALL         0x000003FFU

// -------------------------------------------------------------
// 4. BUTTON DEFINITIONS & EVENTS
// -------------------------------------------------------------
#define QBTN_UP     (1U << 0)
#define QBTN_OK     (1U << 1)
#define QBTN_DOWN   (1U << 2)
#define QBTN_CANCEL (1U << 3)

typedef enum {
    QEVT_BTN_DOWN = 0,
    QEVT_BTN_UP,
    QEVT_BTN_SHORT_CLICK,
    QEVT_BTN_LONG_HOLD
} QButtonEvent;

// -------------------------------------------------------------
// 5. UNIFIED HARDWARE TELEMETRY STRUCT
// -------------------------------------------------------------
typedef struct {
    // === MPU-6500 6-Axis Motion & Attitude ===
    float pitch;        // degrees (-90 to +90)
    float roll;         // degrees (-180 to +180)
    float yaw;          // degrees (0 to 360)
    float accel_x;      // Gs
    float accel_y;      // Gs
    float accel_z;      // Gs
    float gyro_x;       // deg/s
    float gyro_y;       // deg/s
    float gyro_z;       // deg/s

    // === QMC5883P 3D Magnetometer / Compass ===
    float mag_x;        // microTesla / raw Gauss
    float mag_y;        // microTesla / raw Gauss
    float mag_z;        // microTesla / raw Gauss
    float heading;      // 0.0 to 359.9 degrees (tilt-compensated)
    bool  mag_calibrated;

    // === MAX30102 Pulse Oximeter & Health ===
    uint16_t heart_rate_bpm;   // Filtered Heart Rate (BPM)
    uint8_t  spo2_pct;         // Blood Oxygen (0-100%)
    bool     finger_detected;  // Optical contact detection
    float    max30102_temp_c;  // Die temperature
    uint32_t ppg_raw_red;      // Live Red LED ADC count
    uint32_t ppg_raw_ir;       // Live IR LED ADC count

    // === BME280 Environmental ===
    float temperature_c;       // Ambient temperature (°C)
    float humidity_pct;        // Relative humidity (%RH)
    float pressure_hpa;        // Barometric pressure (hPa)
    float altitude_m;          // Calculated altitude (meters)

    // === Power & System Telemetry ===
    uint8_t  battery_pct;      // 0 to 100%
    uint16_t battery_mv;       // Raw battery voltage in mV
    bool     is_charging;

    // === Real-Time Clock ===
    uint8_t  hour;             // 0-23
    uint8_t  minute;           // 0-59
    uint8_t  second;           // 0-59
    uint8_t  day;              // 1-31
    uint8_t  month;            // 1-12
    uint16_t year;             // e.g. 2026
} QTelemetry;

// -------------------------------------------------------------
// 6. MASTER Q-WATCH HARDWARE EXPORT TABLE (API)
// -------------------------------------------------------------
typedef struct QWatchAPI {
    uint32_t api_version;
    uint32_t supported_caps;

    // === OLED DISPLAY (128x64 SH1106 SPI) ===
    uint8_t* (*get_framebuffer)(void);
    void (*draw_pixel)(int16_t x, int16_t y, uint8_t color);
    void (*draw_line)(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
    void (*draw_rect)(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color, bool fill);
    void (*draw_circle)(int16_t x, int16_t y, int16_t r, uint8_t color, bool fill);
    void (*draw_string)(int16_t x, int16_t y, const char* str, uint8_t font_size);
    void (*draw_bitmap)(int16_t x, int16_t y, int16_t w, int16_t h, const uint8_t* bitmap);
    void (*clear_screen)(void);
    void (*flush_display)(void);

    // === INPUTS ===
    uint8_t (*get_button_state)(void); // Bitmask of QBTN_*

    // === TELEMETRY & SENSORS ===
    void (*get_telemetry)(QTelemetry* out);
    void (*enable_health_sensor)(bool enable);
    void (*get_ppg_buffer)(uint32_t* red_buf, uint32_t* ir_buf, uint16_t count);

    // === INFRARED SUBSYSTEM ===
    void (*ir_send_raw)(const uint16_t* timings, uint16_t length, uint16_t khz);
    void (*ir_send_nec)(uint32_t address, uint32_t command);
    bool (*ir_has_received)(void);
    bool (*ir_get_received)(uint32_t* protocol, uint32_t* address, uint32_t* command);

    // === AUDIO & BUZZER ===
    void (*play_tone)(uint16_t freq_hz, uint16_t duration_ms);
    void (*stop_tone)(void);

    // === WS2812 ONBOARD RGB LED ===
    void (*set_led)(uint8_t r, uint8_t g, uint8_t b);

    // === SYSTEM & TIME ===
    uint32_t (*millis)(void);
    uint32_t (*micros)(void);
    void (*delay_ms)(uint32_t ms);
    uint32_t (*random_range)(uint32_t min, uint32_t max);

    // === STORAGE (LittleFS App Sandbox) ===
    int (*file_read)(const char* path, void* buf, uint32_t max_len);
    int (*file_write)(const char* path, const void* buf, uint32_t len);
    bool (*file_exists)(const char* path);

    // === LOGGING & LIFECYCLE ===
    void (*log_print)(const char* msg);
    void (*exit_app)(void);
} QWatchAPI;

// -------------------------------------------------------------
// 7. IN-MEMORY RUNTIME APP DESCRIPTOR
// -------------------------------------------------------------
typedef struct {
    uint32_t magic;             // Must be QAPP_MAGIC (0x51415050)
    uint32_t api_version;       // Targeted API version (e.g. 1)
    uint32_t required_caps;     // Bitmask of required QAPP_CAP_*
    char     name[20];          // Null-terminated display name
    char     version[8];        // e.g. "1.0.0"
    char     author[16];        // e.g. "007 Agent"
    uint32_t required_psram;    // Extra PSRAM bytes requested (0 if none)

    // Lifecycle Entry Points (Relocated in memory)
    int  (*init)(const QWatchAPI* api);              // Returns QAPP_OK or error
    void (*update)(float dt);                         // Frame physics/logic
    void (*render)(void);                             // Render to display
    void (*on_button)(uint8_t btn, QButtonEvent evt); // Input event
    void (*teardown)(void);                           // Cleanup on exit
} QAppHeader;

// -------------------------------------------------------------
// 8. RELOCATION TYPES & RECORD FORMAT
// -------------------------------------------------------------
typedef enum {
    QRELOC_NONE        = 0,
    QRELOC_CODE_ADDR   = 1,   // Target word points to code section: *target += (uintptr_t)code_base
    QRELOC_DATA_ADDR   = 2,   // Target word points to data section: *target += (uintptr_t)data_base
    QRELOC_API_TABLE   = 3    // Target word points to QWatchAPI: *target = (uintptr_t)api_ptr
} QRelocType;

typedef struct {
    uint8_t  section;   // 0 = target word is in code section, 1 = target word is in data section
    uint8_t  type;      // QRelocType
    uint16_t reserved;
    uint32_t offset;    // Byte offset within target section
} QAppReloc;

// -------------------------------------------------------------
// 9. ON-DISK RELOCATABLE FILE HEADER (Zero raw function pointers)
// -------------------------------------------------------------
typedef struct {
    uint32_t magic;             // Must be QAPP_MAGIC (0x51415050)
    uint32_t api_version;       // Targeted API version (1)
    uint32_t required_caps;     // Bitmask of required QAPP_CAP_*
    char     name[20];          // Null-terminated display name
    char     version[8];        // e.g. "1.0.0"
    char     author[16];        // e.g. "007 Agent"
    uint32_t required_psram;    // Extra PSRAM bytes requested for heap/state

    // Relocatable Code & Data Layout
    uint32_t code_offset;       // Byte offset in file where executable code starts
    uint32_t code_size;         // Size of executable code section (placed in IRAM)
    uint32_t data_offset;       // Byte offset in file where data/rodata starts
    uint32_t data_size;         // Size of initialized data section (placed in PSRAM)
    uint32_t bss_size;          // Size of uninitialized data (BSS, zeroed in PSRAM)

    // Relocation & Entry Points
    uint32_t reloc_offset;      // Byte offset where QAppReloc array starts
    uint32_t reloc_count;       // Number of relocation records
    uint32_t entry_offset;      // Byte offset of entry function relative to code start
    uint32_t reserved[2];       // Alignment / Future expansion
} QAppFileHeader;

// Signature for the relocated entry point function
typedef const QAppHeader* (*QAppEntryFunc)(const QWatchAPI* api);

#ifdef __cplusplus
}
#endif

#endif // QWATCH_API_H
