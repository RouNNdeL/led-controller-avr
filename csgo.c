#include <string.h>
#include "csgo.h"
#include "color_utils.h"
#include "eeprom.h"

void process_csgo(csgo_control *control, game_state *state, game_state *old_state, uint8_t *fan, uint8_t fan_start_led,
                  uint8_t *gpu, uint8_t *pc, uint8_t *strip)
{
    //<editor-fold desc="Ammunition on CPU fan">
    /* Number from 0-255*led_count, used to make the effect smooth */
    uint8_t fan_black = 0;
    if(state->weapon_slot == 1 || state->weapon_slot == 2)
    {
        //<editor-fold desc="1, 2 slot">
        uint8_t transition_ammo =
                old_state->ammo - (old_state->ammo - state->ammo) * control->ammo_frame / AMMO_TRANSITION_TIME;
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
                                               (uint16_t) UINT8_MAX / (AMMO_FADE_START - AMMO_FADE_END));
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
        digital_effect(BREATHE, fan, FAN_LED_COUNT, 0, control->bomb_slot_frame, times, args, colors, 1, 1);
    }
    else if(state->weapon_slot)
    {
        set_all_colors(fan, AMMO_COLOR_NO_AMMO, FAN_LED_COUNT);
    }
    else
    {
        fan_black = 1;
        set_all_colors(fan, COLOR_BLACK, FAN_LED_COUNT);
    }
    //</editor-fold>

    //<editor-fold desc="Health on PC">
    uint8_t transition_health =
            old_state->health - (old_state->health - state->health) * control->health_frame / HEALTH_TRANSITION_TIME;

    uint8_t health_colors[9];
    set_color(health_colors, 0, HEALTH_COLOR_HIGH);
    set_color(health_colors, 1, HEALTH_COLOR_MEDIUM);
    set_color(health_colors, 2, HEALTH_COLOR_LOW);

    //Transition to damage color to then smoothly fade out with the GPU
    if(!state->health)
    {
        uint8_t damage_multiplied = control->damage * (uint16_t) DAMAGE_BRIGHTNESS_MULTIPLIER < UINT8_MAX ?
                                    control->damage * DAMAGE_BRIGHTNESS_MULTIPLIER : UINT8_MAX;
        uint8_t color[] = {DAMAGE_COLOR};
        health_colors[6] = color[0] * damage_multiplied / UINT8_MAX;
        health_colors[7] = color[1] * damage_multiplied / UINT8_MAX;
        health_colors[8] = color[2] * damage_multiplied / UINT8_MAX;
    }

    if(transition_health > HEALTH_MEDIUM_HEALTH)
    {
        cross_fade(pc, health_colors, 3, 0,
                   (transition_health - HEALTH_MEDIUM_HEALTH) * (uint16_t) UINT8_MAX / (255 - HEALTH_MEDIUM_HEALTH));
    }
    else
    {
        cross_fade(pc, health_colors, 6, 3, transition_health * (uint16_t) UINT8_MAX / HEALTH_MEDIUM_HEALTH);
    }
    //</editor-fold>

    //<editor-fold desc="Damage on GPU">

    if(control->damage)
    {
        if(!control->damage_animate && control->damage_buffer_frame >= DAMAGE_BUFFER_TIME &&
           control->damage_transition_frame >= DAMAGE_TRANSITION_TIME)
        {
            control->damage_animate = 1;
            control->damage_frame = 0;
        }
        if(control->damage_animate &&
           control->damage_frame >= control->damage * (uint16_t) DAMAGE_ANIMATION_TIME / UINT8_MAX)
        {
            control->damage_animate = 0;
            control->damage = 0;
            control->damage_previous = 0;
        }
    }

    if(control->damage)
    {
        uint8_t damage_multiplied = control->damage * (uint16_t) DAMAGE_BRIGHTNESS_MULTIPLIER < UINT8_MAX ?
                                    control->damage * DAMAGE_BRIGHTNESS_MULTIPLIER : UINT8_MAX;

        if(control->damage_animate)
        {
            uint16_t times[] = {0, 0, 0, control->damage * (uint16_t) DAMAGE_ANIMATION_TIME / UINT8_MAX};
            uint8_t args[] = {0, 0, damage_multiplied};
            uint8_t color[] = {DAMAGE_COLOR};
            simple_effect(BREATHE, gpu, control->damage_frame, times, args, color, 1, 1, 0);
            if(!state->health)
                memcpy(pc, gpu, 3);
        }
        else
        {
            if(control->damage_frame + 1 >= DAMAGE_ANIMATION_START)
            {
                uint8_t color[] = {DAMAGE_COLOR};
                uint8_t damage_multiplied_previous =
                        control->damage_previous * (uint16_t) DAMAGE_BRIGHTNESS_MULTIPLIER < UINT8_MAX ?
                        control->damage_previous * DAMAGE_BRIGHTNESS_MULTIPLIER : UINT8_MAX;
                uint8_t damage_transition = damage_multiplied_previous ?
                                            damage_multiplied_previous +
                                            (damage_multiplied - damage_multiplied_previous) *
                                            control->damage_transition_frame / DAMAGE_TRANSITION_TIME
                                                                       : damage_multiplied;
                gpu[0] = color[0] * damage_transition / UINT8_MAX;
                gpu[1] = color[1] * damage_transition / UINT8_MAX;
                gpu[2] = color[2] * damage_transition / UINT8_MAX;

                if(!state->health)
                    memcpy(pc, gpu, 3);
            }
            else
            {
                uint16_t times[] = {0, DAMAGE_ANIMATION_START};
                uint8_t args[] = {0, 0, damage_multiplied};
                uint8_t color[] = {DAMAGE_COLOR};
                simple_effect(BREATHE, gpu, control->damage_frame, times, args, color, 1, 1, 0);
            }
        }
    }
    else
    {
        set_color(gpu, 0, COLOR_BLACK);
        if(!state->health)
            set_color(pc, 0, COLOR_BLACK);
    }


    //</editor-fold>

    if(state->bomb_state == BOMB_PLANTED)
    {
        if(control->bomb_tick_rate > 0)
        {
            if(control->bomb_overall_frame >= 2440)
            {
                uint8_t analog_base[9] = {pc[0], pc[1], pc[2], gpu[0], gpu[1], gpu[2], BOMB_TICK_COLOR};
                uint8_t progress = control->bomb_frame * (uint16_t) UINT8_MAX / control->bomb_tick_rate * 2;
                cross_fade(pc, analog_base, 0, 6, progress);
                cross_fade(gpu, analog_base, 3, 6, progress);

                uint8_t fan_cpy[FAN_LED_COUNT * 3 + 3];
                memcpy(fan_cpy, fan, FAN_LED_COUNT * 3);
                fan_cpy[FAN_LED_COUNT * 3] = analog_base[6];
                fan_cpy[FAN_LED_COUNT * 3 + 1] = analog_base[7];
                fan_cpy[FAN_LED_COUNT * 3 + 2] = analog_base[8];
                for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
                {
                    cross_fade(fan + i * 3, fan_cpy, i * 3, FAN_LED_COUNT * 3, progress);
                }

                if(control->bomb_frame >= control->bomb_tick_rate)
                {
                    control->bomb_tick_rate = 0;
                }
            }
            else
            {
                uint8_t analog_base[12] = {pc[0], pc[1], pc[2], gpu[0], gpu[1], gpu[2], BOMB_TICK_COLOR,
                                           BOMB_TICK_COLOR_BACKUP};
                uint8_t progress = control->bomb_frame * (uint16_t) UINT8_MAX / control->bomb_tick_rate;
                progress = (progress > INT8_MAX ? UINT8_MAX - progress : progress) * 2;
                cross_fade(pc, analog_base, 0, is_black(pc) ? 9 : 6, progress);
                cross_fade(gpu, analog_base, 3, is_black(gpu) ? 9 : 6, progress);

                uint8_t fan_cpy[FAN_LED_COUNT * 3 + 3];
                memcpy(fan_cpy, fan, FAN_LED_COUNT * 3);
                fan_cpy[FAN_LED_COUNT * 3] = analog_base[6 + (fan_black ? 3 : 0)];
                fan_cpy[FAN_LED_COUNT * 3 + 1] = analog_base[6 + (fan_black ? 3 : 0)];
                fan_cpy[FAN_LED_COUNT * 3 + 2] = analog_base[8 + (fan_black ? 3 : 0)];
                for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
                {
                    cross_fade(fan + i * 3, fan_cpy, i * 3, FAN_LED_COUNT * 3, progress);
                }
            }
        }
        else
        {
            set_color(gpu, 0, BOMB_BEEP_COLOR);
            set_color(pc, 0, BOMB_BEEP_COLOR);
            set_all_colors(fan, BOMB_BEEP_COLOR, FAN_LED_COUNT);
        }
    }

    //<editor-fold desc="Round end">
    if(state->round_state == ROUND_WIN_T)
    {
        uint8_t color[] = {COLOR_T};
        uint16_t times[] = {0, ROUND_END_TRANSITION_TIME, 6000};
        uint8_t args[] = {0, 0, 255};
        simple_effect(BREATHE, pc, control->round_state_frame, times, args, color, 1, 1, 0);
        simple_effect(BREATHE, gpu, control->round_state_frame, times, args, color, 1, 1, 0);
        digital_effect(BREATHE, fan, FAN_LED_COUNT, 0, control->round_state_frame, times, args, color, 1, 1);
    }
    else if(state->round_state == ROUND_WIN_CT)
    {
        uint8_t color[] = {COLOR_CT};
        uint16_t times[] = {0, ROUND_END_TRANSITION_TIME, 6000};
        uint8_t args[] = {0, 0, 255};
        simple_effect(BREATHE, pc, control->round_state_frame, times, args, color, 1, 1, 0);
        simple_effect(BREATHE, gpu, control->round_state_frame, times, args, color, 1, 1, 0);
        digital_effect(BREATHE, fan, FAN_LED_COUNT, 0, control->round_state_frame, times, args, color, 1, 1);
    }
    //</editor-fold>

    //<editor-fold desc="Flash">
    uint8_t transition_flash;
    uint8_t avoid_black = 0;
    if(state->round_state == ROUND_FREEZETIME)
    {
        avoid_black = 1;
        transition_flash = FREEZETIME_STRENGTH -
                           (((FREEZETIME_STRENGTH * (uint32_t) UINT8_MAX * control->round_state_frame) / UINT8_MAX) /
                            FREEZETIME_TIME);
    }
    else
    {
        transition_flash = old_state->flashed - (old_state->flashed - state->flashed) * control->flash_frame /
                                                (old_state->flashed > state->flashed ? FLASH_TRANSITION_TIME_DOWN
                                                                                     : FLASH_TRANSITION_TIME_UP);
    }

    uint8_t analog_base[9] = {pc[0], pc[1], pc[2], gpu[0], gpu[1], gpu[2], FLASH_COLOR};
    if(!(avoid_black && is_black(pc))) cross_fade(pc, analog_base, 0, 6, transition_flash);
    if(!(avoid_black && is_black(gpu))) cross_fade(gpu, analog_base, 3, 6, transition_flash);

    if(!(fan_black && avoid_black))
    {
        uint8_t fan_cpy[FAN_LED_COUNT * 3 + 3];
        memcpy(fan_cpy, fan, FAN_LED_COUNT * 3);
        fan_cpy[FAN_LED_COUNT * 3] = analog_base[6];
        fan_cpy[FAN_LED_COUNT * 3 + 1] = analog_base[7];
        fan_cpy[FAN_LED_COUNT * 3 + 2] = analog_base[8];

        for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
        {
            cross_fade(fan + i * 3, fan_cpy, i * 3, FAN_LED_COUNT * 3, transition_flash);
        }
    }
    //</editor-fold>

    //<editor-fold desc="Underglow">
    //TODO: Add proper support
    set_all_colors(strip, color_from_buf(pc), STRIP_LED_COUNT);
    //</editor-fold>

    //<editor-fold desc="Variable resets">
    if(control->bomb_slot_frame >= BOMB_ANIMATION_TIME)
    {
        control->bomb_slot_frame = 0;
    }
    if(state->weapon_slot != old_state->weapon_slot)
    {
        old_state->ammo = state->ammo;
        old_state->weapon_slot = state->weapon_slot;
    }
    if((state->weapon_slot != 1 && state->weapon_slot != 2) || control->ammo_frame >= AMMO_TRANSITION_TIME)
    {
        old_state->ammo = state->ammo;
    }
    if(control->health_frame >= HEALTH_TRANSITION_TIME)
    {
        old_state->health = state->health;
    }
    if(control->flash_frame >=
       (old_state->flashed > state->flashed ? FLASH_TRANSITION_TIME_DOWN : FLASH_TRANSITION_TIME_UP))
    {
        old_state->flashed = state->flashed;
    }
    if(control->damage_transition_frame >= DAMAGE_TRANSITION_TIME)
    {
        control->damage_previous = control->damage;
    }
    if(state->bomb_state == BOMB_PLANTED && control->bomb_tick_rate && control->bomb_frame >= control->bomb_tick_rate)
    {
        control->bomb_frame = 0;
        control->bomb_tick_rate -= BOMB_TICK_DECREASE;
    }
    //</editor-fold>
}