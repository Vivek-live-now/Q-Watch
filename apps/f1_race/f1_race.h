#ifndef F1_RACE_H
#define F1_RACE_H

#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns pointer to the compiled QAppHeader for F1 Race
const QAppHeader* get_f1_race_header(void);

// App Lifecycle Entry Points
int  f1_race_init(const QWatchAPI* api);
void f1_race_update(float dt);
void f1_race_render(void);
void f1_race_on_button(uint8_t btn, QButtonEvent evt);
void f1_race_teardown(void);

// Query helpers for tests
int   f1_race_get_score(void);
float f1_race_get_speed(void);
bool  f1_race_is_game_over(void);

#ifdef __cplusplus
}
#endif

#endif // F1_RACE_H
