#ifndef BREAKOUT_H
#define BREAKOUT_H

#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns pointer to the compiled QAppHeader for Breakout
const QAppHeader* get_breakout_header(void);

// App Lifecycle Entry Points
int  breakout_init(const QWatchAPI* api);
void breakout_update(float dt);
void breakout_render(void);
void breakout_on_button(uint8_t btn, QButtonEvent evt);
void breakout_teardown(void);

// Query helpers for tests
int  breakout_get_score(void);
int  breakout_get_lives(void);
int  breakout_get_bricks_left(void);
bool breakout_is_game_over(void);

#ifdef __cplusplus
}
#endif

#endif // BREAKOUT_H
