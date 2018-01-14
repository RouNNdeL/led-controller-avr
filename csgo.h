#include <avr/io.h>

#ifndef LEDCONTROLLER_CSGO_H
#define LEDCONTROLLER_CSGO_H

#define AMMO_COLOR_NORMAL rgb(255, 180, 0)
#define AMMO_COLOR_LOW COLOR_RED
#define AMMO_COLOR_NO_AMMO rgb(122, 213, 255)
#define AMMO_FADE_START 51 /* When to start fade from COLOR_NORMAL to COLOR_LOW */
#define AMMO_FADE_END 25
#define AMMO_TRANSITION_TIME 10 /* in frames*/

#define BOMB_SLOT_COLOR COLOR_RED
#define BOMB_ANIMATION_TIME 64 /* in frames */
#define BOMB_ANIMATION_LOW 150 /* in frames */
#define BOMB_ANIMATION_HIGH 255 /* in frames */

#define HEALTH_COLOR_HIGH COLOR_GREEN
#define HEALTH_COLOR_MEDIUM COLOR_YELLOW
#define HEALTH_COLOR_LOW COLOR_RED
#define HEALTH_MEDIUM_HEALTH 128
#define HEALTH_TRANSITION_TIME 16 /* in frames*/

#define FLASH_COLOR COLOR_WHITE
#define FLASH_TRANSITION_TIME_UP 4
#define FLASH_TRANSITION_TIME_DOWN 8

#define DAMAGE_COLOR COLOR_RED
#define DAMAGE_BRIGHTNESS_MULTIPLIER 5 / 2
#define DAMAGE_TRANSITION_TIME 8
#define DAMAGE_ANIMATION_START 16
#define DAMAGE_ANIMATION_TIME 192
#define DAMAGE_BUFFER_TIME 32

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
    uint8_t damage;
    uint8_t damage_previous;
    uint16_t damage_frame;
    uint16_t damage_transition_frame;
    uint16_t damage_buffer_frame;
    uint8_t damage_animate;
    uint8_t bomb_tick_frame;
} csgo_control;

#define csgo_increment_frames() csgo_ctrl.ammo_frame++;\
csgo_ctrl.health_frame++;\
csgo_ctrl.bomb_tick_frame++;\
csgo_ctrl.flash_frame++;\
csgo_ctrl.damage_frame++;\
csgo_ctrl.damage_transition_frame++;\
csgo_ctrl.damage_buffer_frame++

#define CSGO_STATE_LENGTH sizeof(game_state)

void process_csgo(csgo_control *control, game_state *state, game_state *old_state, uint8_t *fan, uint8_t fan_start_led,
                  uint8_t *gpu, uint8_t *pc);

#endif //LEDCONTROLLER_CSGO_H
