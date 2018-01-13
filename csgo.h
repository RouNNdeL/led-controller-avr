#include <avr/io.h>

#ifndef LEDCONTROLLER_CSGO_H
#define LEDCONTROLLER_CSGO_H

#define AMMO_COLOR_NORMAL rgb(255, 180, 0)
#define AMMO_COLOR_LOW rgb(255, 0, 0)
#define AMMO_COLOR_NO_AMMO rgb(155, 226, 255)
#define AMMO_FADE_START 51 /* When to start fade from COLOR_NORMAL to COLOR_LOW */
#define AMMO_FADE_END 25
#define AMMO_TRANSITION_TIME 10 /* in frames*/

#define BOMB_SLOT_COLOR rgb(255, 25, 25)
#define BOMB_ANIMATION_TIME 64 /* in frames */
#define BOMB_ANIMATION_LOW 150 /* in frames */
#define BOMB_ANIMATION_HIGH 255 /* in frames */

#define HEALTH_COLOR_HIGH rgb(0, 255, 0)
#define HEALTH_COLOR_MEDIUM rgb(255, 255, 0)
#define HEALTH_COLOR_LOW rgb(255, 0, 0)
#define HEALTH_MEDIUM_HEALTH 128
#define HEALTH_TRANSITION_TIME 16 /* in frames*/

#define FLASH_COLOR COLOR_WHITE
#define FLASH_TRANSITION_TIME 8

typedef struct
{
    uint8_t health;
    uint8_t flashed;
    uint8_t ammo;
    uint8_t weapon_slot;
    uint8_t bomb_progress;
} __attribute__((packed)) game_state;


typedef struct
{
    uint8_t ammo_frame;
    uint8_t flash_frame;
    uint8_t health_frame;
    uint8_t bomb_tick;
} control_frames;



#define CSGO_STATE_LENGTH sizeof(game_state)

void process_csgo(control_frames *frames, game_state *state, game_state *old_state, uint8_t *fan, uint8_t fan_start_led,
                  uint8_t *gpu, uint8_t *pc);

#endif //LEDCONTROLLER_CSGO_H
