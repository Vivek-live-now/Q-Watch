#ifndef SPACE_IMPACT_H
#define SPACE_IMPACT_H

#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns pointer to the compiled QAppHeader for Space Impact 2
const QAppHeader* get_space_impact_header(void);

// App Lifecycle Entry Points
int  space_impact_init(const QWatchAPI* api);
void space_impact_update(float dt);
void space_impact_render(void);
void space_impact_on_button(uint8_t btn, QButtonEvent evt);
void space_impact_teardown(void);

// Query helpers for test harnesses
int  space_impact_get_score(void);
int  space_impact_get_high_score(void);
int  space_impact_get_lives(void);
int  space_impact_get_bombs(void);
bool space_impact_is_boss_active(void);
bool space_impact_is_game_over(void);

#ifdef __cplusplus
}
#endif

#endif // SPACE_IMPACT_H
