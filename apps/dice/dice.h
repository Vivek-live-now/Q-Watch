#ifndef DICE_H
#define DICE_H

#include <stdint.h>
#include <stdbool.h>
#include "../../include/qwatch_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DICE_MODE_D6 = 0,
    DICE_MODE_D20,
    DICE_MODE_D100,
    DICE_MODE_COIN,
    DICE_MODE_2D6,
    DICE_MODE_3D6,
    DICE_MODE_D4,
    DICE_MODE_D8,
    DICE_MODE_D12,
    DICE_MODE_COUNT
} DiceMode;

// Exported QApp Header for dynamic relocatable Micro-ELF loader
const QAppHeader* get_dice_header(void);

// App Lifecycle Entry Points
int  dice_init(const QWatchAPI* api);
void dice_update(float dt);
void dice_render(void);
void dice_on_button(uint8_t btn, QButtonEvent evt);
void dice_teardown(void);

// Inspection helpers for host unit testing
DiceMode dice_get_mode(void);
void     dice_set_mode(DiceMode mode);
int      dice_get_last_result(void);
bool     dice_is_rolling(void);
void     dice_trigger_roll(void);
int      dice_get_history(int index); // 0 to 3

#ifdef __cplusplus
}
#endif

#endif // DICE_H
