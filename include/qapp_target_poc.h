#ifndef QAPP_TARGET_POC_H
#define QAPP_TARGET_POC_H

#include <stdint.h>
#include <stdbool.h>
#include "qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool iram_allocated;
    bool psram_allocated;
    bool iram_address_valid;
    bool psram_address_valid;
    bool code_relocated;
    bool executed_from_iram;
    bool psram_state_mutated;
    bool api_call_succeeded;
    bool resources_freed;
    uint32_t execution_result;
} QAppPocResult;

// Runs the target-side ESP32-S3 relocatable IRAM/PSRAM proof-of-concept
QAppPocResult run_target_esp32s3_poc(const QWatchAPI* api);

#ifdef __cplusplus
}
#endif

#endif // QAPP_TARGET_POC_H
