#ifndef COMPASS_HUD_H
#define COMPASS_HUD_H

#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns pointer to the compiled QAppHeader for the Compass HUD
const QAppHeader* get_compass_hud_header(void);

// App Lifecycle Entry Points
int  compass_hud_init(const QWatchAPI* api);
void compass_hud_update(float dt);
void compass_hud_render(void);
void compass_hud_on_button(uint8_t btn, QButtonEvent evt);
void compass_hud_teardown(void);

// Compass HUD state queries for testing and verification
float   compass_hud_get_heading(void);
float   compass_hud_get_target_bearing(void);
bool    compass_hud_is_target_locked(void);
int16_t compass_hud_get_declination(void);
bool    compass_hud_is_calibrated(void);
float   compass_hud_get_field_strength(void);

#ifdef __cplusplus
}
#endif

#endif // COMPASS_HUD_H
