#include "csgo.h"
#include "color_utils.h"
#include "eeprom.h"

void process_csgo(uint32_t frame, game_state state, uint8_t* fan, uint8_t fan_start_led, uint8_t* gpu, uint8_t* pc)
{
    //<editor-fold desc="Ammunition on CPU fan">
    /* Number from 0-255*led_count, used to make the effect smooth */
    uint8_t color[3];
    if(state.ammo >= AMMO_TRANSITION_START)
    {
        set_color(color, 0, AMMO_COLOR_NORMAL);
    }
    else if(state.ammo > AMMO_TRANSITION_END)
    {
        uint8_t temp[6];
        set_color(temp, 0, AMMO_COLOR_NORMAL);
        set_color(temp, 1, AMMO_COLOR_LOW);
        cross_fade(color, temp, 3, 0, state.ammo * (uint32_t) UINT16_MAX / AMMO_TRANSITION_END);
    }
    else
    {
        set_color(color, 0, AMMO_COLOR_LOW);
    }

    uint16_t led_progress = state.ammo * FAN_LED_COUNT;

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

    set_all_colors(pc, 0, rgb(0,0,0));
    set_all_colors(gpu, 0, rgb(0,0,0));
    //</editor-fold>
}