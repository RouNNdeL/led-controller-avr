#include "csgo.h"
#include "color_utils.h"
#include "eeprom.h"

void process_csgo(control_frames frames, game_state* state, game_state* old_state, uint8_t *fan, uint8_t fan_start_led,
                  uint8_t *gpu, uint8_t *pc)
{
    //<editor-fold desc="Ammunition on CPU fan">
    /* Number from 0-255*led_count, used to make the effect smooth */
    uint8_t transition_ammo = (*old_state).ammo - ((*old_state).ammo - (*state).ammo) * frames.ammo_frame / AMMO_TRANSITION_TIME;
    uint8_t color[3];
    if(transition_ammo >= AMMO_FADE_START)
    {
        set_color(color, 0, AMMO_COLOR_NORMAL);
    }
    else if(transition_ammo > AMMO_FADE_END)
    {
        uint8_t temp[6];
        set_color(temp, 0, AMMO_COLOR_NORMAL);
        set_color(temp, 1, AMMO_COLOR_LOW);
        cross_fade(color, temp, 3, 0, (transition_ammo - AMMO_FADE_END) *
                (uint32_t) UINT16_MAX / (AMMO_FADE_START - AMMO_FADE_END));
    }
    else
    {
        set_color(color, 0, AMMO_COLOR_LOW);
    }

    uint16_t led_progress = transition_ammo * FAN_LED_COUNT;

    for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
    {
        uint8_t index = (i + fan_start_led) % FAN_LED_COUNT * 3;

        if(led_progress >= UINT8_MAX)
        {
            fan[index] = color[0];
            fan[index + 1] = color[1];
            fan[index + 2] = color[2];

            led_progress -= UINT8_MAX;
        }
        else if(led_progress > 0)
        {

            fan[index] = color[0] * led_progress / UINT8_MAX;
            fan[index + 1] = color[1] * led_progress / UINT8_MAX;
            fan[index + 2] = color[2] * led_progress / UINT8_MAX;


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
    
    set_color(pc, 0, rgb(0, 0, 0));
    set_color(gpu, 0, rgb(0, 0, 0));

    if(frames.ammo_frame >= AMMO_TRANSITION_TIME)
    {
        (*old_state).ammo = (*state).ammo;
    }
}