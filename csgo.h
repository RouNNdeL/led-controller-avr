#include <avr/io.h>

#ifndef LEDCONTROLLER_CSGO_H
#define LEDCONTROLLER_CSGO_H

#define AMMO_COLOR_NORMAL rgb(255, 180, 0)
#define AMMO_COLOR_LOW rgb(255, 0, 0)
#define AMMO_TRANSITION_START 51
#define AMMO_TRANSITION_END 25

typedef struct
{
    uint8_t health;
    uint8_t ammo;
    uint8_t bomb_progress;
} __attribute__((packed)) game_state;

#define CSGO_STATE_LENGTH sizeof(game_state)

void process_csgo(uint32_t frame, game_state state, uint8_t* fan, uint8_t fan_start_led, uint8_t* gpu, uint8_t* pc);

#endif //LEDCONTROLLER_CSGO_H
