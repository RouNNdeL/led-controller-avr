#include <avr/io.h>
#include "color_utils.h"

uint8_t actual_brightness(uint8_t brightness)
{
    return (brightness * brightness) / 255;
}

void set_color(uint8_t *p_buf, uint8_t led, uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t index = 3 * led;
    p_buf[index++] = r;
    p_buf[index++] = g;
    p_buf[index] = b;
}

void set_all_colors(uint8_t *p_buf, uint8_t r, uint8_t g, uint8_t b, uint8_t count)
{
    for(uint8_t i = 0; i < count; ++i)
    {
        set_color(p_buf, i, r, g, b);
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

/**
 * @param effect - effect to calculate
 * @param color - an array of length 3 that the color is going to be assigned to
 * @param frame - a frame to calculate the color for
 * @param times - timing arguments resolved to frames
 * @param colors - an array of length 24 containing 16*4 colors
 *               in a proper configuration for set device (fans are always GRB)
 * @param color_count - how many colors are in use
 * @return - a color to be displayed at a given frame
 */
void simple_effect(effect effect, uint8_t *color, uint32_t frame, uint16_t *times, uint8_t *colors, uint8_t color_count)
{
    uint32_t sum = times[0] + times[1] + times[2] + times[3];
    uint32_t d_time = frame % sum;
    uint8_t n_color = ((frame / sum) % color_count);
    uint8_t m_color = (n_color == color_count - 1) ? 0 : n_color + 1;
    n_color *= 3;
    m_color *= 3;

    if((d_time) < times[0])
    {
        if(effect == BREATHE)
        {
            color[0] = 0x00;
            color[1] = 0x00;
            color[2] = 0x00;
        }
    }
    else if((d_time -= times[0]) < times[1])
    {
        uint16_t progress  = d_time * UINT16_MAX  / times[1];

        if(effect == BREATHE)
        {
            color[0] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[1] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[2] = colors[n_color] * (uint32_t) progress / UINT16_MAX;
        }
        else if(effect == FADE)
        {
            if(colors[n_color] > colors[m_color])
                color[0] = colors[n_color] - (colors[n_color++] - colors[m_color++]) * (uint32_t) progress / UINT16_MAX;
            else
                color[0] = colors[n_color] + (colors[m_color++] - colors[n_color++]) * (uint32_t) progress / UINT16_MAX;

            if(colors[n_color] > colors[m_color])
                color[1] = colors[n_color] - (colors[n_color++] - colors[m_color++]) * (uint32_t) progress / UINT16_MAX;
            else
                color[1] = colors[n_color] + (colors[m_color++] - colors[n_color++]) * (uint32_t) progress / UINT16_MAX;

            if(colors[n_color] > colors[m_color])
                color[2] = colors[n_color] - (colors[n_color] - colors[m_color]) * (uint32_t) progress / UINT16_MAX;
            else
                color[2] = colors[n_color] + (colors[m_color] - colors[n_color]) * (uint32_t) progress / UINT16_MAX;
        }
    }
    else if((d_time -= times[1]) < times[2])
    {
        if(effect == BREATHE)
        {
            color[0] = colors[n_color++];
            color[1] = colors[n_color++];
            color[2] = colors[n_color];
        }
        else if(effect == FADE)
        {
            color[0] = colors[m_color++];
            color[1] = colors[m_color++];
            color[2] = colors[m_color];
        }
    }
    else if((d_time -= times[2]) < times[3])
    {
        uint16_t progress = UINT16_MAX - d_time * UINT16_MAX /times[3];

        if(effect == BREATHE)
        {
            color[0] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[1] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[2] = colors[n_color] * (uint32_t) progress / UINT16_MAX;
        }
    }
}

#pragma clang diagnostic pop

void adv_effect(effect effect, uint8_t *leds, uint8_t count, uint8_t offset, uint32_t frame,
                uint16_t *times, uint8_t *colors, uint8_t color_count)
{
    uint32_t sum = times[0] + times[1] + times[2] + times[3];
    uint32_t d_time = frame % sum;
    uint8_t n_color = ((frame / sum) % color_count);
    uint8_t m_color = (n_color == color_count - 1) ? 0 : n_color + 1;
    n_color *= 3;
    //m_color *= 3;

    leds += offset;

    if((d_time) < times[0])
    {
        set_all_colors(leds, 0x00, 0x00, 0x00, count);
    }
    else if((d_time -= times[0]) < times[1])
    {
        uint16_t progress = d_time * UINT16_MAX / times[1];

        if(effect == FILL)
        {
            uint8_t count_on = (progress * (uint32_t) count + UINT16_MAX/2) / UINT16_MAX;

            for(uint8_t i = 0; i < count; i += 1)
            {
                uint8_t index = i * 3;
                if(i <= count_on)
                {
                    leds[index] = colors[n_color];
                    leds[index + 1] = colors[n_color + 1];
                    leds[index + 2] = colors[n_color + 2];
                }
                else
                {
                    leds[index] = 0x00;
                    leds[index + 1] = 0x00;
                    leds[index + 2] = 0x00;
                }
            }
        }
    }
    else if((d_time -= times[1]) < times[2])
    {
        set_all_colors(leds, colors[n_color], colors[n_color + 1], colors[n_color + 2], count);
    }
    else if((d_time -= times[2]) < times[3])
    {
        uint16_t progress = d_time * UINT16_MAX / times[3];

        if(effect == FILL)
        {
            uint8_t count_on = (progress * (uint32_t) count + UINT16_MAX/2) / UINT16_MAX;

            for(uint8_t i = 0; i < count; ++i)
            {
                uint8_t index = i * 3;

                if(i > count_on)
                {
                    leds[index] = colors[n_color];
                    leds[index + 1] = colors[n_color + 1];
                    leds[index + 2] = colors[n_color + 2];
                }
                else
                {
                    leds[index] = 0x00;
                    leds[index + 1] = 0x00;
                    leds[index + 2] = 0x00;
                }
            }
        }
    }
}