#ifndef MOCK_QWATCH_API_H
#define MOCK_QWATCH_API_H

#include "../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns the singleton mock QWatchAPI table
const QWatchAPI* get_mock_qwatch_api(void);

// Mock harness inspection and simulation helpers
void mock_reset_state(void);
void mock_set_telemetry(const QTelemetry* telem);
void mock_set_buttons(uint8_t btn_mask);
uint8_t* mock_get_display_buffer(void);
uint16_t mock_get_last_tone_freq(void);
uint16_t mock_get_last_tone_duration(void);
void mock_get_last_led(uint8_t* r, uint8_t* g, uint8_t* b);
bool mock_is_exit_requested(void);
bool mock_is_health_sensor_enabled(void);
int mock_get_flush_count(void);

#ifdef __cplusplus
}
#endif

#endif // MOCK_QWATCH_API_H
