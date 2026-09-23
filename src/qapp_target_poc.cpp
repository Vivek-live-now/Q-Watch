#include "qapp_target_poc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <esp_heap_caps.h>

#if __has_include(<esp_rom_spiflash.h>)
#include <esp_rom_spiflash.h>
#define QAPP_FLUSH_ICACHE() esp_rom_spiflash_cache_flush()
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ESP32S3)
#if __has_include("esp32s3/rom/cache.h")
#include "esp32s3/rom/cache.h"
#define QAPP_FLUSH_ICACHE() Cache_Invalidate_ICache_All()
#else
extern "C" void Cache_Invalidate_ICache_All(void);
#define QAPP_FLUSH_ICACHE() Cache_Invalidate_ICache_All()
#endif
#elif defined(CONFIG_IDF_TARGET_ESP32)
#if __has_include("esp32/rom/cache.h")
#include "esp32/rom/cache.h"
#define QAPP_FLUSH_ICACHE() Cache_Flush(0)
#else
extern "C" void Cache_Flush(int);
#define QAPP_FLUSH_ICACHE() Cache_Flush(0)
#endif
#else
#define QAPP_FLUSH_ICACHE() do {} while(0)
#endif
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

// Prototype for template function used to extract genuine target machine code
typedef uint32_t (*PocWorkerFunc)(uint32_t* state, const QWatchAPI* api);

static uint32_t __attribute__((noinline, aligned(4))) poc_worker_template(uint32_t* state, const QWatchAPI* api) {
    if (!state) return 0;
    *state += 58; // 42 + 58 = 100
    if (api && api->random_range) {
        return *state + api->random_range(10, 10); // 100 + 10 = 110
    }
    return *state;
}

// Marker function to measure template code size
static void __attribute__((noinline, aligned(4))) poc_worker_end_marker(void) {
    // Empty marker placed immediately after template
}

QAppPocResult run_target_esp32s3_poc(const QWatchAPI* api) {
    QAppPocResult res;
    memset(&res, 0, sizeof(res));

    // Calculate machine code size of template function
    uintptr_t fn_start = (uintptr_t)&poc_worker_template;
    uintptr_t fn_end = (uintptr_t)&poc_worker_end_marker;
    size_t code_size = 128;
    if (fn_end > fn_start && (fn_end - fn_start) < 256) {
        code_size = fn_end - fn_start;
    }

#ifdef ARDUINO
    // -------------------------------------------------------------
    // ESP32-S3 TARGET EXECUTION PATH
    // -------------------------------------------------------------

    // 1. Allocate Executable Internal Memory (IRAM)
    // MALLOC_CAP_EXEC ensures instruction-bus address (0x40378000 - 0x403E0000)
    uint8_t* iram_code = (uint8_t*)heap_caps_malloc(
        code_size + 16,
        MALLOC_CAP_EXEC | MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL
    );

    if (iram_code) {
        res.iram_allocated = true;
        uintptr_t addr = (uintptr_t)iram_code;
        // Verify address is in genuine ESP32-S3 IRAM range
        if (addr >= 0x40378000 && addr < 0x403E0000) {
            res.iram_address_valid = true;
        }
    }

    // 2. Allocate App State in External 2MB PSRAM
    // MALLOC_CAP_SPIRAM ensures placement in PSRAM (0x3C000000+)
    uint32_t* psram_state = nullptr;
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >= 64) {
        psram_state = (uint32_t*)heap_caps_malloc(64, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!psram_state) {
        // Fallback to internal DRAM if PSRAM not enabled on board
        psram_state = (uint32_t*)malloc(64);
    }

    if (psram_state) {
        res.psram_allocated = true;
        uintptr_t paddr = (uintptr_t)psram_state;
        // Verify address is in PSRAM or DRAM data bus range
        if ((paddr >= 0x3C000000 && paddr < 0x3E000000) || (paddr >= 0x3FC88000 && paddr < 0x3FCE0000)) {
            res.psram_address_valid = true;
        }
        *psram_state = 42; // Seed initial state
    }

    if (res.iram_address_valid && res.psram_allocated) {
        // 3. Load Xtensa machine code into executable IRAM
        memcpy(iram_code, (const void*)fn_start, code_size);
        res.code_relocated = true;

        // 4. Invalidate instruction cache so CPU fetches new instructions
        QAPP_FLUSH_ICACHE();
#if defined(__XTENSA__)
        asm volatile("isync\n\tmemw\n\t");
#endif

        // 5. Execute directly from the dynamically allocated IRAM memory
        PocWorkerFunc dynamic_func = (PocWorkerFunc)iram_code;
        uint32_t retval = dynamic_func(psram_state, api);

        res.execution_result = retval;
        res.executed_from_iram = true;

        // 6. Verify PSRAM state mutation and API resolution
        if (*psram_state == 100) {
            res.psram_state_mutated = true;
        }
        if (retval == 110) {
            res.api_call_succeeded = true;
        }
    }

    // 7. Safe Teardown & Resource Cleanup
    if (iram_code) {
        heap_caps_free(iram_code);
    }
    if (psram_state) {
        free(psram_state);
    }
    res.resources_freed = true;

#else
    // -------------------------------------------------------------
    // HOST SIMULATOR EXECUTION PATH (Using POSIX mmap PROT_EXEC)
    // -------------------------------------------------------------

    // 1. Allocate real executable page using mmap
    size_t page_size = sysconf(_SC_PAGESIZE);
    uint8_t* exec_mem = (uint8_t*)mmap(
        NULL, page_size,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0
    );

    if (exec_mem != MAP_FAILED) {
        res.iram_allocated = true;
        res.iram_address_valid = true;
    }

    // 2. Allocate data state (simulating PSRAM data bus)
    uint32_t* host_state = (uint32_t*)malloc(64);
    if (host_state) {
        res.psram_allocated = true;
        res.psram_address_valid = true;
        *host_state = 42;
    }

    if (res.iram_allocated && res.psram_allocated) {
        // 3. Copy machine code into the executable page
        memcpy(exec_mem, (const void*)fn_start, code_size);
        __builtin___clear_cache((char*)exec_mem, (char*)exec_mem + page_size);
        res.code_relocated = true;

        // 4. Execute function from the dynamically allocated page
        PocWorkerFunc dynamic_func = (PocWorkerFunc)exec_mem;
        uint32_t retval = dynamic_func(host_state, api);

        res.execution_result = retval;
        res.executed_from_iram = true;

        // 5. Verify state mutation and API resolution
        if (*host_state == 100) {
            res.psram_state_mutated = true;
        }
        if (retval == 110) {
            res.api_call_succeeded = true;
        }
    }

    // 6. Safe Teardown
    if (exec_mem != MAP_FAILED) {
        munmap(exec_mem, page_size);
    }
    if (host_state) {
        free(host_state);
    }
    res.resources_freed = true;

#endif

    return res;
}
