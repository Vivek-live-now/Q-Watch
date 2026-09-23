#ifndef TILT_GAME_H
#define TILT_GAME_H

#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns pointer to the compiled QAppHeader for the Tilt Game
const QAppHeader* get_tilt_game_header(void);

// App Lifecycle Entry Points
int  tilt_game_init(const QWatchAPI* api);
void tilt_game_update(float dt);
void tilt_game_render(void);
void tilt_game_on_button(uint8_t btn, QButtonEvent evt);
void tilt_game_teardown(void);

// Game state queries for testing
float tilt_game_get_ball_x(void);
float tilt_game_get_ball_y(void);
int   tilt_game_get_score(void);

#ifdef __cplusplus
}
#endif

#endif // TILT_GAME_H
