#include <string.h>
#include "csgo.h"
#include "color_utils.h"
#include "eeprom.h"

void process_csgo(control_frames *frames, game_state *state, game_state *old_state, uint8_t *fan, uint8_t fan_start_led,
                  uint8_t *gpu, uint8_t *pc)
{
    //<editor-fold desc="Ammunition on CPU fan">
    /* Number from 0-255*led_count, used to make the effect smooth */
    if(state->weapon_slot == 1 || state->weapon_slot == 2)
    {
        //<editor-fold desc="1, 2 slot">
        uint8_t transition_ammo =
                old_state->ammo - (old_state->ammo - state->ammo) * frames->ammo_frame / AMMO_TRANSITION_TIME;
        uint8_t ammo_color[3];
        if(transition_ammo >= AMMO_FADE_START)
        {
            set_color(ammo_color, 0, AMMO_COLOR_NORMAL);
        }
        else if(transition_ammo > AMMO_FADE_END)
        {
            uint8_t temp[6];
            set_color(temp, 0, AMMO_COLOR_NORMAL);
            set_color(temp, 1, AMMO_COLOR_LOW);
            cross_fade(ammo_color, temp, 3, 0, (transition_ammo - AMMO_FADE_END) *
                                               (uint32_t) UINT16_MAX / (AMMO_FADE_START - AMMO_FADE_END));
        }
        else
        {
            set_color(ammo_color, 0, AMMO_COLOR_LOW);
        }

        uint16_t led_progress = transition_ammo * FAN_LED_COUNT;

        for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
        {
            uint8_t index = (i + fan_start_led) % FAN_LED_COUNT * 3;

            if(led_progress >= UINT8_MAX)
            {
                fan[index] = ammo_color[0];
                fan[index + 1] = ammo_color[1];
                fan[index + 2] = ammo_color[2];

                led_progress -= UINT8_MAX;
            }
            else if(led_progress > 0)
            {

                fan[index] = ammo_color[0] * led_progress / UINT8_MAX;
                fan[index + 1] = ammo_color[1] * led_progress / UINT8_MAX;
                fan[index + 2] = ammo_color[2] * led_progress / UINT8_MAX;


                led_progress = 0;
            }
            else
            {
                fan[index] = 0x00;
                fan[index + 1] = 0x00;
                fan[index + 2] = 0x00;
            }
        }
        //</editor-fold>
    }
    else if(state->weapon_slot == 5)
    {
        uint16_t times[] = {0, BOMB_ANIMATION_TIME / 2, 0, BOMB_ANIMATION_TIME / 2, 0, 0};
        uint8_t args[] = {0, BOMB_ANIMATION_LOW, BOMB_ANIMATION_HIGH};
        uint8_t colors[] = {BOMB_SLOT_COLOR};
        digital_effect(BREATHE, fan, FAN_LED_COUNT, 0, frames->bomb_tick, times, args, colors, 1, 1);
    }
    else if(state->weapon_slot)
    {
        set_all_colors(fan, AMMO_COLOR_NO_AMMO, FAN_LED_COUNT);
    }
    else
    {
        set_all_colors(fan, COLOR_BLACK, FAN_LED_COUNT);
    }
    //</editor-fold>

    //<editor-fold desc="Health on PC">
    uint8_t transition_health =
            old_state->health - (old_state->health - state->health) * frames->health_frame / HEALTH_TRANSITION_TIME;

    uint8_t health_colors[9];
    set_color(health_colors, 0, HEALTH_COLOR_HIGH);
    set_color(health_colors, 1, HEALTH_COLOR_MEDIUM);
    set_color(health_colors, 2, HEALTH_COLOR_LOW);

    if(transition_health > HEALTH_MEDIUM_HEALTH)
    {
        cross_fade(pc, health_colors, 3, 0,
                   (transition_health - HEALTH_MEDIUM_HEALTH) * (uint32_t) UINT16_MAX / (255 - HEALTH_MEDIUM_HEALTH));
    }
    else
    {
        cross_fade(pc, health_colors, 6, 3, transition_health * (uint32_t) UINT16_MAX / HEALTH_MEDIUM_HEALTH);
    }
    //</editor-fold>

    set_color(gpu, 0, rgb(0, 0, 0));

    //<editor-fold desc="Flash">
    uint8_t transition_flash =
            old_state->flashed - (old_state->flashed - state->flashed) * frames->flash_frame / FLASH_TRANSITION_TIME;
    uint16_t transition_progress = transition_flash * UINT8_MAX;

    uint8_t pc_base[6] = {pc[0], pc[1], pc[2], FLASH_COLOR};
    cross_fade(pc, pc_base, 0, 3, transition_progress);
    
    uint8_t fan_cpy[FAN_LED_COUNT * 3 + 3];
    memcpy(fan_cpy, fan, FAN_LED_COUNT * 3);
    fan_cpy[FAN_LED_COUNT * 3] = pc_base[3];
    fan_cpy[FAN_LED_COUNT * 3 + 1] = pc_base[4];
    fan_cpy[FAN_LED_COUNT * 3 + 2] = pc_base[5];

    for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
    {
        cross_fade(fan + i * 3, fan_cpy, i * 3, FAN_LED_COUNT * 3, transition_progress);
    }
    //</editor-fold>

    if(frames->bomb_tick >= BOMB_ANIMATION_TIME)
    {
        frames->bomb_tick = 0;
    }
    if(state->weapon_slot != old_state->weapon_slot)
    {
        old_state->ammo = state->ammo;
        old_state->weapon_slot = state->weapon_slot;
    }
    if((state->weapon_slot != 1 && state->weapon_slot != 2) || frames->ammo_frame >= AMMO_TRANSITION_TIME)
    {
        old_state->ammo = state->ammo;
    }
    if(frames->health_frame >= HEALTH_TRANSITION_TIME)
    {
        old_state->health = state->health;
    }
    if(frames->flash_frame >= FLASH_TRANSITION_TIME)
    {
        old_state->flashed = state->flashed;
    }
}