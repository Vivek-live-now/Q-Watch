#ifndef PACMAN_H
#define PACMAN_H

#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns pointer to the compiled QAppHeader for Pacman
const QAppHeader* get_pacman_header(void);

// App Lifecycle Entry Points
int  pacman_init(const QWatchAPI* api);
void pacman_update(float dt);
void pacman_render(void);
void pacman_on_button(uint8_t btn, QButtonEvent evt);
void pacman_teardown(void);

// Query helpers for tests
int  pacman_get_score(void);
int  pacman_get_lives(void);
int  pacman_get_dots_left(void);
bool pacman_is_game_over(void);

#ifdef __cplusplus
}
#endif

#endif // PACMAN_H
