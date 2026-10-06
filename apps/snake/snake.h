#ifndef SNAKE_H
#define SNAKE_H

#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns pointer to the compiled QAppHeader for Snake
const QAppHeader* get_snake_header(void);

// App Lifecycle Entry Points
int  snake_init(const QWatchAPI* api);
void snake_update(float dt);
void snake_render(void);
void snake_on_button(uint8_t btn, QButtonEvent evt);
void snake_teardown(void);

// Query helpers for tests
int snake_get_score(void);
int snake_get_high_score(void);
int snake_get_length(void);
bool snake_is_game_over(void);

#ifdef __cplusplus
}
#endif

#endif // SNAKE_H
