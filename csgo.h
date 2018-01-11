#include <avr/io.h>

#ifndef LEDCONTROLLER_CSGO_H
#define LEDCONTROLLER_CSGO_H

typedef struct
{
    uint8_t health;
    uint8_t ammo;
    uint8_t bomb_progress;
} __attribute__((packed)) game_state;

void process_csgo(uint32_t frame, game_state state, uint8_t* fan, uint8_t* gpu, uint8_t* pc);

#endif //LEDCONTROLLER_CSGO_H
