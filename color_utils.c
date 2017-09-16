#include <avr/io.h>
#include <unwind.h>
#include "color_utils.h"

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

void cross_fade(uint8_t *color, uint8_t *colors, uint8_t n_color, uint8_t m_color, uint16_t progress)
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

void rainbow_at_progress_full(uint8_t *color, uint16_t progress, uint8_t brightness)
{
    if(progress <= 10922)
    {
        color[0] = 255;
        color[1] = 0;
        color[2] = progress / 43;
    }
    else if((progress -= 10923) <= 10923)
    {
        color[0] = UINT8_MAX - progress / 43;
        color[1] = 0;
        color[2] = 255;
    }
    else if((progress -= 10922) <= 10922)
    {
        color[0] = 0;
        color[1] = progress / 43;
        color[2] = 255;
    }
    else if((progress -= 10923) <= 10923)
    {
        color[0] = 0;
        color[1] = 255;
        color[2] = UINT8_MAX - progress / 43;
    }
    else if((progress -= 10922) <= 10922)
    {
        color[0] = progress / 43;
        color[1] = 255;
        color[2] = 0;
    }
    else if((progress -= 10923) <= 10923)
    {
        color[0] = 255;
        color[1] = UINT8_MAX - progress / 43;
        color[2] = 0;
    }

    color[0] = color[0] * brightness / UINT8_MAX;
    color[1] = color[1] * brightness / UINT8_MAX;
    color[2] = color[2] * brightness / UINT8_MAX;
}

void rainbow_at_progress(uint8_t *color, uint16_t progress, uint8_t brightness)
{
    if(progress <= 21845)
    {
        uint8_t val = progress / 86;
        color[0] = UINT8_MAX - val;
        color[1] = 0;
        color[2] = val;
    }
    else if((progress -= 21845) <= 21845)
    {
        uint8_t val = progress / 86;
        color[0] = 0;
        color[1] = val;
        color[2] = UINT8_MAX - val;
    }
    else if((progress -= 21845) <= 21845)
    {
        uint8_t val = progress / 86;
        color[0] = val;
        color[1] = UINT8_MAX - val;
        color[2] = 0;
    }

    color[0] = color[0] * brightness / UINT8_MAX;
    color[1] = color[1] * brightness / UINT8_MAX;
    color[2] = color[2] * brightness / UINT8_MAX;
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

/**
 * @param effect - effect to calculate
 * @param color - an array of length 3 that the color is going to be assigned to
 * @param frame - a frame to calculate the color for
 * @param times - timing arguments resolved to frames
 * @param colors - an array of length 24 containing 16*4 colors
 *               in a RGB order
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
        uint16_t progress = d_time * UINT16_MAX / times[1];

        if(effect == BREATHE)
        {
            color[0] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[1] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[2] = colors[n_color] * (uint32_t) progress / UINT16_MAX;
        }
        else if(effect == FADE)
        {
            cross_fade(color, colors, n_color, m_color, progress);
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
        uint16_t progress = UINT16_MAX - d_time * UINT16_MAX / times[3];

        if(effect == BREATHE)
        {
            color[0] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[1] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[2] = colors[n_color] * (uint32_t) progress / UINT16_MAX;
        }
    }
}

#pragma clang diagnostic pop

/**
 * Function to calculate the effects for addressable LEDs
 *
 * Arguments for effects:<ul>
 * <li>FILL, FADE - {bit_packed*, NONE, NONE, NONE}</li>
 * <li>RAINBOW - {bit_packed*, brightness, mode, NONE}</li>
 * <li>PIECES - {bit_packed* (dir implemented yet), color_count, piece_count, NONE}</li>
 * <li>ROTATING - {bit_packed* (dir implemented yet), color_count, element_count, led_count}</li></ul>
 * 
 * * - We pack 1 bit values to allow for more arguments:<ul>
 * <li>DIRECTION - 0</li>
 * <li>SMOOTH - 1</li></ul>
 *
 * @param effect - effect to calculate
 * @param leds - a pointer to an RGB array
 * @param led_count - number of LEDs in the 'leds' pointer
 * @param offset - offset for the pointer
 * @param frame - a frame to calculate the effect for
 * @param times - timing arguments resolved to frames
 * @param args - additional arguments for the effects, described above
 * @param colors - an array of length 24 containing 16*4 colors
 *               in a RGB order
 * @param color_count - how many colors are in use
 */
void adv_effect(effect effect, uint8_t *leds, uint8_t led_count, uint8_t start_led, uint32_t frame,
                uint16_t *times, uint8_t *args, uint8_t *colors, uint8_t color_count)
{
    if(effect != PIECES && effect != ROTATING)
    {
        uint32_t sum = times[0] + times[1] + times[2] + times[3];
        uint32_t d_time = frame % sum;
        uint8_t n_color = ((frame / sum) % color_count);
        uint8_t m_color = (n_color == color_count - 1) ? 0 : n_color + 1;
        n_color *= 3;
        m_color *= 3;

        if((d_time) < times[0])
        {
            set_all_colors(leds, 0x00, 0x00, 0x00, led_count);
        }
        else if((d_time -= times[0]) < times[1])
        {
            /* A 16bit replace for a float*/
            uint16_t progress = d_time * UINT16_MAX / times[1];

            if(effect == FILL)
            {
                /* Number from 0-255*led_count, used to make the effect smooth */
                uint16_t led_progress = (progress * (uint32_t) led_count) / UINT8_MAX;

                for(uint8_t i = 0; i < led_count; ++i)
                {
                    uint8_t index = (((args[0] & DIRECTION) ? i : led_count - i - 1) + start_led) % led_count * 3;

                    if(led_progress >= UINT8_MAX)
                    {
                        leds[index] = colors[n_color];
                        leds[index + 1] = colors[n_color + 1];
                        leds[index + 2] = colors[n_color + 2];


                        led_progress -= UINT8_MAX;
                    }
                    else if(led_progress > 0 && (args[0] & SMOOTH))
                    {

                        leds[index] = colors[n_color] * led_progress / UINT8_MAX;
                        leds[index + 1] = colors[n_color + 1] * led_progress / UINT8_MAX;
                        leds[index + 2] = colors[n_color + 2] * led_progress / UINT8_MAX;


                        led_progress = 0;
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
            set_all_colors(leds, colors[n_color], colors[n_color + 1], colors[n_color + 2], led_count);
        }
        else if((d_time -= times[2]) < times[3])
        {
            uint16_t progress = UINT16_MAX - d_time * UINT16_MAX / times[3];

            uint16_t led_progress = effect != RAINBOW ?
                                    (progress * (uint32_t) led_count) / UINT8_MAX :
                                    UINT16_MAX / led_count; /* How much of progress is one LED */

            for(uint8_t i = 0; i < led_count; ++i)
            {
                uint8_t index = (((args[0] & DIRECTION) ? led_count - i - 1 : i) + start_led) % led_count * 3;
                uint8_t alt_index = (((args[0] & DIRECTION) ? i : led_count - i - 1) + start_led) % led_count * 3;

                if(effect != RAINBOW)
                {
                    if(led_progress >= UINT8_MAX)
                    {
                        leds[index] = colors[n_color];
                        leds[index + 1] = colors[n_color + 1];
                        leds[index + 2] = colors[n_color + 2];

                        led_progress -= UINT8_MAX;
                    }
                    else if(led_progress > 0 && (args[0] & SMOOTH))
                    {
                        if(effect == FILL)
                        {
                            leds[index] = colors[n_color] * led_progress / UINT8_MAX;
                            leds[index + 1] = colors[n_color + 1] * led_progress / UINT8_MAX;
                            leds[index + 2] = colors[n_color + 2] * led_progress / UINT8_MAX;
                        }
                        else
                        {
                            cross_fade(leds + index, colors, m_color, n_color, led_progress * UINT8_MAX);
                        }

                        led_progress = 0;
                    }
                    else
                    {
                        if(effect == FILL)
                        {
                            leds[index] = 0x00;
                            leds[index + 1] = 0x00;
                            leds[index + 2] = 0x00;
                        }
                        else
                        {
                            leds[index] = colors[m_color];
                            leds[index + 1] = colors[m_color + 1];
                            leds[index + 2] = colors[m_color + 2];
                        }
                    }
                }
                else
                {
                    uint16_t d_progress = (progress + i * led_progress) * args[3] % UINT16_MAX;
                    if(args[2])
                        rainbow_at_progress_full(leds + alt_index, d_progress, args[1]);
                    else
                        rainbow_at_progress(leds + alt_index, d_progress, args[1]);
                }
            }
        }
    }
    else
    {
        uint16_t sum = times[2] + times[3];
        uint32_t d_time = frame % sum;
        uint8_t c_count = effect == ROTATING ? led_count : args[1];
        uint8_t c_colors[c_count * 3];
        uint8_t n_color = ((frame / sum) % (color_count / args[1])) * args[1];
        uint8_t m_color = (n_color + args[1]) % color_count;
        n_color *= 3;
        m_color *= 3;

        /* Generate our colors used in this frame */
        if((d_time) < times[2])
        {
            uint8_t count = 0;
            uint8_t c_leds = 0;
            for(uint8_t i = 0; i < c_count; ++i)
            {
                uint8_t index = i * 3;
                if(effect == ROTATING && (i % (led_count / args[2])) >= args[3])
                {
                    c_colors[index] = 0;
                    c_colors[index + 1] = 0;
                    c_colors[index + 2] = 0;
                }
                else
                {
                    uint8_t c_index = count * 3;
                    c_colors[index] = colors[n_color + c_index];
                    c_colors[index + 1] = colors[n_color + c_index + 1];
                    c_colors[index + 2] = colors[n_color + c_index + 2];
                    c_leds++;
                    if(effect == ROTATING && c_leds >= args[3])
                    {
                        count = (count + 1) % args[1];
                        c_leds = 0;
                    }
                }
            }
        }
        else if((d_time -= times[2]) < times[3])
        {
            /* Crossfade based on progress if needed */
            uint16_t progress = d_time * UINT16_MAX / times[3];
            uint8_t count = 0;
            uint8_t c_leds = 0;

            for(uint8_t i = 0; i < c_count; ++i)
            {
                uint8_t index = i * 3;
                if(effect == ROTATING && (i % (led_count / args[2])) >= args[3])
                {
                    c_colors[index] = 0;
                    c_colors[index + 1] = 0;
                    c_colors[index + 2] = 0;
                }
                else
                {
                    uint8_t c_index = count * 3;
                    cross_fade(c_colors+index, colors, n_color+c_index, m_color+c_index, progress);
                    c_leds++;
                    if(effect != ROTATING || c_leds >= args[3])
                    {
                        count = (count + 1) % args[1];
                        c_leds = 0;
                    }
                }
            }
        }

        //TODO: Possibly optimize those calculations
        uint16_t rotation_progress = times[0] ? frame % times[0] * UINT16_MAX / times[0] : 0;

        /* Which LED is the start led */
        uint8_t led_offset = (rotation_progress / (UINT16_MAX / led_count)) % led_count;
        /* What's left from the previous LED */
        uint8_t led_carry =
                (uint32_t) rotation_progress * UINT8_MAX / (UINT16_MAX / led_count) - UINT8_MAX * led_offset;
        /* How many LEDs per piece (*255) */
        uint16_t piece_leds = (effect == ROTATING ? 1 : led_count / args[2]) * UINT8_MAX;
        uint16_t current_leds = 0;
        uint8_t color = 0;

        for(uint8_t j = 0; j < led_count; ++j)
        {
            //TODO: Add direction argument support
            uint8_t index = (((args[0] & DIRECTION) ? j + led_offset :
                              led_count + j + 1 - led_offset) + start_led) % led_count * 3;

            /* If we're at the first LED of a certain color and led_carry != 0 crossfade with the previous color */
            if(current_leds == 0 && led_carry && (args[0] & SMOOTH))
            {
                cross_fade(leds + index, c_colors, color * 3,
                           ((color + c_count - 1) % c_count) * 3, led_carry * UINT8_MAX);

                current_leds = led_carry;
            }
            else if((current_leds += UINT8_MAX) <= piece_leds)
            {
                leds[index] = c_colors[color * 3];
                leds[index + 1] = c_colors[color * 3 + 1];
                leds[index + 2] = c_colors[color * 3 + 2];
            }
            else
            {
                color = (color + 1) % c_count; /* Next color */
                current_leds = 0; /* Reset current counter */
                j--; /* Backtrack to crossfade that LED */
            }
        }
    }
}