#ifndef INVADERS_H
#define INVADERS_H

#include <stdint.h>
#include <stdbool.h>
#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Exported QApp Header for dynamic relocatable Micro-ELF loader
const QAppHeader* get_invaders_header(void);

// App Lifecycle Entry Points
int  invaders_init(const QWatchAPI* api);
void invaders_update(float dt);
void invaders_render(void);
void invaders_on_button(uint8_t btn, QButtonEvent evt);
void invaders_teardown(void);

// Inspection helpers for host unit testing
int   invaders_get_score(void);
int   invaders_get_high_score(void);
int   invaders_get_lives(void);
int   invaders_get_wave(void);
int   invaders_get_aliens_alive(void);
float invaders_get_player_x(void);
void  invaders_fire_player_laser(void);
void  invaders_reset_game(void);

#ifdef __cplusplus
}
#endif

#endif // INVADERS_H
