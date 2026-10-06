#ifndef BOUNCE_H
#define BOUNCE_H

#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns pointer to the compiled QAppHeader for Nokia Bounce
const QAppHeader* get_bounce_header(void);

// App Lifecycle Entry Points
int  bounce_init(const QWatchAPI* api);
void bounce_update(float dt);
void bounce_render(void);
void bounce_on_button(uint8_t btn, QButtonEvent evt);
void bounce_teardown(void);

// Query helpers for test harnesses
int   bounce_get_score(void);
int   bounce_get_high_score(void);
int   bounce_get_lives(void);
int   bounce_get_rings_left(void);
int   bounce_get_level(void);
float bounce_get_ball_x(void);
float bounce_get_ball_y(void);
bool  bounce_is_game_over(void);

#ifdef __cplusplus
}
#endif

#endif // BOUNCE_H
