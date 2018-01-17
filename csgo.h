#include <avr/io.h>

#ifndef LEDCONTROLLER_CSGO_H
#define LEDCONTROLLER_CSGO_H

#define COLOR_T rgb(255, 113, 12)
#define COLOR_CT rgb(0, 71, 255)

#define AMMO_COLOR_NORMAL rgb(255, 180, 0)
#define AMMO_COLOR_LOW COLOR_RED
#define AMMO_COLOR_NO_AMMO rgb(122, 213, 255)
#define AMMO_FADE_START 51 /* When to start fade from COLOR_NORMAL to COLOR_LOW */
#define AMMO_FADE_END 25
#define AMMO_TRANSITION_TIME 10 /* in frames*/

#define BOMB_SLOT_COLOR COLOR_RED
#define BOMB_ANIMATION_TIME 64 /* in frames */
#define BOMB_ANIMATION_LOW 150
#define BOMB_ANIMATION_HIGH 255

#define HEALTH_COLOR_HIGH COLOR_GREEN
#define HEALTH_COLOR_MEDIUM COLOR_YELLOW
#define HEALTH_COLOR_LOW COLOR_RED
#define HEALTH_MEDIUM_HEALTH 128
#define HEALTH_TRANSITION_TIME 16 /* in frames*/

#define FLASH_COLOR COLOR_WHITE
#define FLASH_TRANSITION_TIME_UP 4 /* in frames*/
#define FLASH_TRANSITION_TIME_DOWN 8 /* in frames*/

#define DAMAGE_COLOR COLOR_RED
#define DAMAGE_MIN_ON_DEATH 50
#define DAMAGE_BRIGHTNESS_MULTIPLIER 5 / 2
#define DAMAGE_TRANSITION_TIME 8 /* in frames*/
#define DAMAGE_ANIMATION_START 16 /* in frames*/
#define DAMAGE_ANIMATION_TIME 192 /* in frames*/
#define DAMAGE_BUFFER_TIME 32 /* in frames*/

#define BOMB_TICK_COLOR COLOR_BLACK
#define BOMB_TICK_COLOR_BACKUP COLOR_RED
#define BOMB_EXPLODE_COLOR COLOR_T
#define BOMB_DEFUSE_COLOR COLOR_CT
#define BOMB_TICK_INIT 100
#define BOMB_TICK_DECREASE 2
#define BOMB_EXPLODE_TIME 12

#define ROUND_END_TRANSITION_TIME 16
#define ROUND_NO_STATE 0
#define ROUND_WIN_T 1
#define ROUND_WIN_CT 2
#define ROUND_FREEZETIME 3

#define FREEZETIME_TIME 960
#define FREEZETIME_STRENGTH 100

#define BOMB_NO_STATE 0
#define BOMB_PLANTED 1
#define BOMB_EXPLODED 2
#define BOMB_DEFUSED 3

typedef struct
{
    uint8_t health;
    uint8_t flashed;
    uint8_t ammo;
    uint8_t weapon_slot;
    uint8_t bomb_state;
    uint8_t round_state;
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
    uint16_t bomb_overall_frame;
    uint16_t bomb_frame;
    uint16_t bomb_tick_rate;
    uint16_t round_state_frame;
} csgo_control;

#define csgo_increment_frames() \
csgo_ctrl.ammo_frame++;\
csgo_ctrl.health_frame++;\
csgo_ctrl.bomb_tick_frame++;\
csgo_ctrl.bomb_overall_frame++;\
csgo_ctrl.bomb_frame++;\
csgo_ctrl.flash_frame++;\
csgo_ctrl.damage_frame++;\
csgo_ctrl.damage_transition_frame++;\
csgo_ctrl.damage_buffer_frame++;\
csgo_ctrl.round_state_frame++

#define CSGO_STATE_LENGTH sizeof(game_state)

void process_csgo(csgo_control *control, game_state *state, game_state *old_state, uint8_t *fan, uint8_t fan_start_led,
                  uint8_t *gpu, uint8_t *pc);

#endif //LEDCONTROLLER_CSGO_H
