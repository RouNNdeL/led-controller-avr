#include <avr/io.h>
#include <unwind.h>
#include <string.h>
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

void rotate_buf(uint8_t *leds, uint8_t led_count, uint16_t rotation_progress, uint8_t start_led, uint16_t piece_leds,
                uint8_t bit_pack, uint8_t *colors, uint8_t color_count)
{
    /* Which LED is the start led */
    uint8_t led_offset = (rotation_progress / (UINT16_MAX / led_count)) % led_count;
    /* What's left from the previous LED */
    uint8_t led_carry =
            (uint32_t) rotation_progress * UINT8_MAX / (UINT16_MAX / led_count) - UINT8_MAX * led_offset;
    /* How many LEDs per piece (*255) */
    uint16_t current_leds = 0;
    uint8_t color = 0;

    for(uint8_t j = 0; j < led_count; ++j)
    {
        //TODO: Add direction argument support
        uint8_t index = (j + led_offset + start_led) % led_count * 3;

        /* If we're at the first LED of a certain color and led_carry != 0 crossfade with the previous color */
        if(current_leds == 0 && led_carry && (bit_pack & SMOOTH))
        {
            cross_fade(leds + index, colors, color * 3,
                       ((color + color_count - 1) % color_count) * 3, led_carry * UINT8_MAX);

            current_leds = led_carry;
        }
        else if((current_leds += UINT8_MAX) <= piece_leds)
        {
            leds[index] = colors[color * 3];
            leds[index + 1] = colors[color * 3 + 1];
            leds[index + 2] = colors[color * 3 + 2];
        }
        else
        {
            color = (color + 1) % color_count; /* Next color */
            current_leds = 0; /* Reset current counter */
            j--; /* Backtrack to crossfade that LED */
        }
    }

    //Not the most effective, but gets the job done. 1st place for future performance improvements
    if(bit_pack & DIRECTION)
    {
        uint8_t i = led_count - 1;
        uint8_t j = 0;
        while(i > j)
        {
            uint8_t index = i * 3;
            uint8_t index2 = j * 3;
            uint8_t r = leds[index];
            uint8_t g = leds[index + 1];
            uint8_t b = leds[index + 2];
            leds[index] = leds[index2];
            leds[index + 1] = leds[index2 + 1];
            leds[index + 2] = leds[index2 + 2];
            leds[index2] = r;
            leds[index2 + 1] = g;
            leds[index2 + 2] = b;
            i--;
            j++;
        }
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
 *               in a RGB order
 * @param color_count - how many colors are in use
 * @return - a color to be displayed at a given frame
 */
void simple_effect(effect effect, uint8_t *color, uint32_t frame, uint16_t *times, uint8_t *args, uint8_t *colors,
                   uint8_t color_count,
                   uint8_t color_cycles)
{
    uint32_t sum = times[0] + times[1] + times[2] + times[3];
    uint32_t d_time = frame % sum;
    uint8_t n_color = ((frame / sum / color_cycles) % color_count);
    uint8_t m_color = (n_color == color_count - 1) ? 0 : n_color + 1;
    n_color *= 3;
    m_color *= 3;

    if((d_time) < times[TIME_OFF])
    {
        color[0] = colors[n_color++] * args[ARG_BREATHE_START] / UINT8_MAX;
        color[1] = colors[n_color++] * args[ARG_BREATHE_START] / UINT8_MAX;
        color[2] = colors[n_color] * args[ARG_BREATHE_START] / UINT8_MAX;
    }
    else if((d_time -= times[TIME_OFF]) < times[TIME_FADEIN])
    {
        uint16_t delta_brightness = args[ARG_BREATHE_END] - args[ARG_BREATHE_START];
        uint16_t progress = d_time * delta_brightness * UINT8_MAX / times[TIME_FADEIN]
                            + (args[ARG_BREATHE_START] * UINT8_MAX);

        color[0] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
        color[1] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
        color[2] = colors[n_color] * (uint32_t) progress / UINT16_MAX;

    }
    else if((d_time -= times[TIME_FADEIN]) < times[TIME_ON])
    {
        if(effect == BREATHE)
        {
            color[0] = colors[n_color++] * args[ARG_BREATHE_END] / UINT8_MAX;
            color[1] = colors[n_color++] * args[ARG_BREATHE_END] / UINT8_MAX;
            color[2] = colors[n_color] * args[ARG_BREATHE_END] / UINT8_MAX;
        }
        else
        {
            color[0] = colors[n_color++];
            color[1] = colors[n_color++];
            color[2] = colors[n_color];
        }
    }
    else if((d_time -= times[TIME_ON]) < times[TIME_FADEOUT])
    {


        if(effect == BREATHE)
        {
            uint16_t delta_brightness = args[ARG_BREATHE_END] - args[ARG_BREATHE_START];
            uint16_t progress = (args[ARG_BREATHE_END] * UINT8_MAX) -
                                (d_time * delta_brightness * UINT8_MAX / times[TIME_FADEOUT]);

            color[0] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[1] = colors[n_color++] * (uint32_t) progress / UINT16_MAX;
            color[2] = colors[n_color] * (uint32_t) progress / UINT16_MAX;
        }
        else
        {
            uint16_t progress = d_time * UINT16_MAX / times[TIME_FADEOUT];
            if(effect == FADE)
            {
                cross_fade(color, colors, n_color, m_color, progress);
            }
            else
            {
                rainbow_at_progress_full(color, progress, args[ARG_RAINBOW_BRIGHTNESS]);
            }
        }
    }
}

#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"

/**
 * Function to calculate the effects for addressable LEDs
 *
 * Arguments for effects:<ul>
 * <li>FILL, FADE - {bit_packed*, NONE, NONE, NONE}</li>
 * <li>RAINBOW - {bit_packed*, brightness, sources, NONE}</li>
 * <li>PIECES - {bit_packed*, color_count, piece_count, NONE}</li>
 * <li>ROTATING - {bit_packed*, color_count, element_count, led_count}</li></ul>
 * 
 * * - We pack 1 bit values to allow for more arguments:<ul>
 * <li>DIRECTION - 0</li>
 * <li>SMOOTH - 1</li>
 * <li>MODE - 2 (for RAINBOW only)</li></ul>
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
void digital_effect(effect effect, uint8_t *leds, uint8_t led_count, uint8_t start_led, uint32_t frame,
                    uint16_t *times, uint8_t *args, uint8_t *colors, uint8_t color_count, uint8_t color_cycles)
{
    if(effect == BREATHE || effect == FADE || (effect == RAINBOW && (args[ARG_BIT_PACK] & RAINBOW_SIMPLE)))
    {
        uint8_t color[3];
        simple_effect(effect, color, frame, times, args, colors, color_count, color_cycles);
        set_all_colors(leds, color[0], color[1], color[2], led_count);
        return;
    }
    if(effect != PIECES && effect != ROTATING)
    {
        uint32_t sum = times[0] + times[1] + times[2] + times[3];
        uint32_t d_time = frame % sum;
        uint8_t n_color = ((frame / sum / color_cycles) % (color_count / args[ARG_FILL_COLOR_COUNT])) *
                          args[ARG_FILL_COLOR_COUNT];
        uint8_t m_color = (n_color + args[ARG_FILL_COLOR_COUNT]) % color_count;
        n_color *= 3;
        m_color *= 3;

        if((d_time) < times[TIME_OFF])
        {
            set_all_colors(leds, 0x00, 0x00, 0x00, led_count);
        }
        else if((d_time -= times[TIME_OFF]) < times[TIME_FADEIN])
        {
            /* A 16bit replace for a float*/
            uint16_t progress = d_time * UINT16_MAX / times[TIME_FADEIN];

            if(effect == FILL)
            {

                uint8_t piece_leds = led_count / args[ARG_FILL_PIECE_COUNT];

                /* Number from 0-255*led_count, used to make the effect smooth */
                uint16_t led_progress_base = (progress * (uint32_t) piece_leds) / UINT8_MAX;
                uint16_t led_progress_current;
                int8_t piece = -1;
                uint8_t arg_number = 0;

                for(uint8_t i = 0; i < led_count; ++i)
                {
                    if(i % piece_leds == 0)
                    {
                        led_progress_current = led_progress_base;
                        piece++;
                        if(piece > 8)
                        {
                            piece = 0;
                            arg_number = 1;
                        }
                    }

                    uint8_t direction = (arg_number ? args[ARG_FILL_PIECE_DIRECTIONS2] :
                                         args[ARG_FILL_PIECE_DIRECTIONS1]) & (1 << piece);
                    uint8_t index = (((direction ? i : led_count - i - 1))
                                     % piece_leds + piece * piece_leds + start_led) % led_count * 3;

                    uint8_t n_color_for_piece = n_color + (3 * piece + 8 * arg_number) % args[ARG_FILL_COLOR_COUNT];
                    if(led_progress_current >= UINT8_MAX)
                    {
                        leds[index] = colors[n_color_for_piece];
                        leds[index + 1] = colors[n_color_for_piece + 1];
                        leds[index + 2] = colors[n_color_for_piece + 2];


                        led_progress_current -= UINT8_MAX;
                    }
                    else if(led_progress_current > 0 && (args[ARG_BIT_PACK] & SMOOTH))
                    {

                        leds[index] = colors[n_color_for_piece] * led_progress_current / UINT8_MAX;
                        leds[index + 1] = colors[n_color_for_piece + 1] * led_progress_current / UINT8_MAX;
                        leds[index + 2] = colors[n_color_for_piece + 2] * led_progress_current / UINT8_MAX;


                        led_progress_current = 0;
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
        else if((d_time -= times[TIME_FADEIN]) < times[TIME_ON])
        {
            uint8_t piece_leds = led_count / args[ARG_FILL_PIECE_COUNT];
            int8_t piece = -1;

            for(uint8_t i = 0; i < led_count; ++i)
            {
                if(i % piece_leds == 0)
                {
                    piece++;
                }

                uint8_t index = (i + start_led) % led_count * 3;

                uint8_t n_color_for_piece = n_color + 3 * piece;
                leds[index] = colors[n_color_for_piece];
                leds[index + 1] = colors[n_color_for_piece + 1];
                leds[index + 2] = colors[n_color_for_piece + 2];
            }
        }
        else if((d_time -= times[TIME_ON]) < times[TIME_FADEOUT])
        {
            uint16_t progress = UINT16_MAX - d_time * UINT16_MAX / times[TIME_FADEOUT];

            uint8_t piece_leds = led_count / args[ARG_FILL_PIECE_COUNT];
            uint16_t led_progress_base = effect != RAINBOW ?
                                         (progress * (uint32_t) piece_leds) / UINT8_MAX :
                                         UINT16_MAX / led_count; /* How much of progress is one LED */

            if(effect != RAINBOW)
            {
                uint16_t led_progress_current = led_progress_base;
                int8_t piece = -1;
                uint8_t arg_number = 0;

                for(uint8_t i = 0; i < led_count; ++i)
                {
                    if(i % piece_leds == 0)
                    {
                        led_progress_current = led_progress_base;
                        piece++;
                        if(piece > 8)
                        {
                            piece = 0;
                            arg_number = 1;
                        }
                    }

                    uint8_t direction = (arg_number ? args[ARG_FILL_PIECE_DIRECTIONS2] :
                                         args[ARG_FILL_PIECE_DIRECTIONS1]) & (1 << piece);
                    uint8_t index =
                            (((direction ^ (args[ARG_BIT_PACK] & FILL_FADE_RETURN ? 1 : 0) ? led_count - i - 1 : i))
                             % piece_leds + piece * piece_leds + start_led) % led_count * 3;

                    uint8_t n_color_for_piece = n_color + (3 * piece + 8 * arg_number) % args[ARG_FILL_COLOR_COUNT];
                    uint8_t m_color_for_piece = m_color + (3 * piece + 8 * arg_number) % args[ARG_FILL_COLOR_COUNT];

                    if(led_progress_current >= UINT8_MAX)
                    {
                        leds[index] = colors[n_color_for_piece];
                        leds[index + 1] = colors[n_color_for_piece + 1];
                        leds[index + 2] = colors[n_color_for_piece + 2];

                        led_progress_current -= UINT8_MAX;
                    }
                    else
                    {
                        if(led_progress_current > 0 && (args[ARG_BIT_PACK] & SMOOTH))
                        {
                            if(effect == FILL)
                            {
                                leds[index] = colors[n_color_for_piece] * led_progress_current / UINT8_MAX;
                                leds[index + 1] = colors[n_color_for_piece + 1] * led_progress_current / UINT8_MAX;
                                leds[index + 2] = colors[n_color_for_piece + 2] * led_progress_current / UINT8_MAX;
                            }
                            else
                            {
                                if(args[ARG_BIT_PACK] & FADE_SMOOTH)
                                {
                                    uint8_t faded[6];
                                    set_color(faded, 0, colors[n_color_for_piece],
                                              colors[n_color_for_piece + 1], colors[n_color_for_piece + 2]);
                                    cross_fade(faded + 3, colors, m_color_for_piece, n_color_for_piece, progress);
                                    cross_fade(leds + index, faded, 3, 0,
                                               led_progress_current * UINT8_MAX);
                                }
                                else
                                {
                                    cross_fade(leds + index, colors, m_color_for_piece, n_color_for_piece,
                                               led_progress_current * UINT8_MAX);
                                }
                            }

                            led_progress_current = 0;
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
                                if(args[ARG_BIT_PACK] & FADE_SMOOTH)
                                {
                                    uint8_t faded[3];
                                    cross_fade(faded, colors, m_color_for_piece, n_color_for_piece, progress);
                                    leds[index] = faded[0];
                                    leds[index + 1] = faded[1];
                                    leds[index + 2] = faded[2];
                                }
                                else
                                {
                                    leds[index] = colors[m_color_for_piece];
                                    leds[index + 1] = colors[m_color_for_piece + 1];
                                    leds[index + 2] = colors[m_color_for_piece + 2];
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                for(uint8_t i = 0; i < led_count; ++i)
                {
                    uint8_t alt_index =
                            (((args[ARG_BIT_PACK] & DIRECTION) ? i : led_count - i - 1) + start_led) % led_count * 3;
                    uint16_t d_progress = (progress + i * led_progress_base) * args[ARG_RAINBOW_SOURCES] % UINT16_MAX;
                    if(args[ARG_BIT_PACK] & RAINBOW_MODE)
                        rainbow_at_progress_full(leds + alt_index, d_progress, args[ARG_RAINBOW_BRIGHTNESS]);
                    else
                        rainbow_at_progress(leds + alt_index, d_progress, args[ARG_RAINBOW_BRIGHTNESS]);
                }
            }

        }
        if(times[TIME_ROTATION])
        {
            uint16_t rotation_progress =
                    ((uint32_t) (frame % times[TIME_ROTATION]) * UINT16_MAX) / times[TIME_ROTATION];
            uint8_t backup[led_count * 3];
            memcpy(backup, leds, led_count * 3);
            rotate_buf(leds, led_count, rotation_progress, 0, UINT8_MAX, args[ARG_BIT_PACK], backup, led_count);
        }
    }
    else
    {
        uint16_t sum = times[TIME_ON] + times[TIME_FADEOUT];
        uint32_t d_time = frame % sum;
        uint8_t c_count = effect == ROTATING ? led_count : args[ARG_PIECES_COLOR_COUNT];
        uint8_t c_colors[c_count * 3];
        uint8_t n_color = ((frame / sum / color_cycles) % (color_count / args[ARG_PIECES_COLOR_COUNT])) *
                          args[ARG_PIECES_COLOR_COUNT];
        uint8_t m_color = (n_color + args[ARG_PIECES_COLOR_COUNT]) % color_count;
        n_color *= 3;
        m_color *= 3;

        /* Generate our colors used in this frame */
        if((d_time) < times[TIME_ON])
        {
            uint8_t count = 0;
            uint8_t c_leds = 0;
            for(uint8_t i = 0; i < c_count; ++i)
            {
                uint8_t index = i * 3;
                if(effect == ROTATING &&
                   (i % (led_count / args[ARG_ROTATING_ELEMENT_COUNT])) >= args[ARG_ROTATING_LED_COUNT])
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
                    if(effect != ROTATING || c_leds >= args[ARG_ROTATING_LED_COUNT])
                    {
                        count = (count + 1) % args[ARG_PIECES_COLOR_COUNT];
                        c_leds = 0;
                    }
                }
            }
        }
        else if((d_time -= times[TIME_ON]) < times[TIME_FADEOUT])
        {
            /* Crossfade based on progress if needed */
            uint16_t progress = d_time * UINT16_MAX / times[TIME_FADEOUT];
            uint8_t count = 0;
            uint8_t c_leds = 0;

            for(uint8_t i = 0; i < c_count; ++i)
            {
                uint8_t index = i * 3;
                if(effect == ROTATING &&
                   (i % (led_count / args[ARG_PIECES_PIECE_COUNT])) >= args[ARG_ROTATING_LED_COUNT])
                {
                    c_colors[index] = 0;
                    c_colors[index + 1] = 0;
                    c_colors[index + 2] = 0;
                }
                else
                {
                    uint8_t c_index = count * 3;
                    cross_fade(c_colors + index, colors, n_color + c_index, m_color + c_index, progress);
                    c_leds++;
                    if(effect != ROTATING || c_leds >= args[ARG_ROTATING_LED_COUNT])
                    {
                        count = (count + 1) % args[ARG_PIECES_COLOR_COUNT];
                        c_leds = 0;
                    }
                }
            }
        }

        //TODO: Possibly optimize those calculations
        uint16_t rotation_progress = times[TIME_ROTATION] ? frame % times[TIME_ROTATION] * UINT16_MAX /
                                                            times[TIME_ROTATION] : 0;
        uint16_t piece_leds = (effect == ROTATING ? 1 : led_count / args[ARG_PIECES_PIECE_COUNT]) * UINT8_MAX;

        rotate_buf(leds, led_count, rotation_progress, start_led, piece_leds, args[ARG_BIT_PACK], c_colors, c_count);
    }
}

void demo_music(uint8_t *fan_buf, uint8_t *pc_buf, uint8_t *gpu_buf, uint32_t frame)
{
    uint8_t colors[12];
    set_color(colors, 0, 255, 0, 0);
    set_color(colors, 1, 0, 255, 0);
    set_color(colors, 2, 0, 0, 255);
    set_color(colors, 3, 255, 255, 255);

    if(frame < 32)
    {
        uint16_t times[] = {8, 0, 0, 24, 0};
        uint8_t args[] = {0, 0, 255, 0, 0};

        digital_effect(BREATHE, fan_buf, 12, 2, frame + 8, times, args, colors + 9, 1, 1);
        simple_effect(BREATHE, gpu_buf, frame + 8, times, args, colors + 9, 1, 1);
        simple_effect(BREATHE, gpu_buf, frame + 8, times, args, colors + 9, 1, 1);
    }
    else if((frame -= 32) < 28)
    {
        uint16_t times1[] = {6, 0, 50, 0, 0};
        uint8_t args1[] = {0, 0, 255, 0, 0};

        set_color(pc_buf, 0, 0, 0, 0);
        set_color(gpu_buf, 0, 0, 0, 0);
        digital_effect(BREATHE, fan_buf, 12, 2, frame, times1, args1, colors, 2, 1);
    }
    else if((frame -= 28) < 28)
    {
        uint16_t times[] = {6, 0, 50, 0, 0};
        uint8_t args[] = {0, 0, 255, 0, 0};

        set_color(pc_buf, 0, 0, 0, 0);
        simple_effect(BREATHE, gpu_buf, frame, times, args, colors + 3, 1, 1);
        set_all_colors(fan_buf, 0, 0, 0, 12);
    }
    else if((frame -= 28) < 28)
    {
        uint16_t times[] = {6, 0, 50, 0, 0};
        uint8_t args[] = {0, 0, 255, 0, 0};

        simple_effect(BREATHE, pc_buf, frame, times, args, colors + 6, 1, 1);
        set_color(gpu_buf, 0, 0, 0, 0);
        set_all_colors(fan_buf, 0, 0, 0, 12);
    }
    else if((frame -= 28) < 28)
    {
        uint16_t times[] = {6, 0, 50, 0, 0};
        uint8_t args[] = {0, 0, 255, 0, 0};

        simple_effect(BREATHE, pc_buf, frame, times, args, colors + 6, 1, 1);
        simple_effect(BREATHE, gpu_buf, frame, times, args, colors + 3, 1, 1);
        digital_effect(BREATHE, fan_buf, 12, 2, frame, times, args, colors, 1, 1);
    }
    else if((frame -= 28) < 112)
    {
        uint16_t times1[] = {0, 16, 0, 16, 0};
        uint8_t args1[] = {0, 20, 255, 0, 0};

        simple_effect(BREATHE, pc_buf, frame + 16, times1, args1, colors + 6, 1, 1);
        simple_effect(BREATHE, gpu_buf, frame + 8, times1, args1, colors + 3, 2, 2);
        digital_effect(BREATHE, fan_buf, 12, 2, frame, times1, args1, colors, 2, 1);
    }
    else if((frame -= 112) < 128)
    {
        uint16_t times1[] = {16, 0, 16, 0, 0};
        uint16_t times2[] = {4, 30, 0, 30, 0};
        uint8_t args1[] = {0, 0, 255, 0, 0};
        uint8_t args2[] = {SMOOTH | DIRECTION, 1, 1, 0, 0};

        set_color(pc_buf, 0, 0, 0, 0);
        simple_effect(BREATHE, gpu_buf, frame + 16, times1, args1, colors + 3, 2, 2);
        digital_effect(FILL, fan_buf, 12, 2, frame, times2, args2, colors, 2, 1);
    }
    else if((frame -= 128) < 128)
    {
        uint16_t times1[] = {16, 0, 8, 0, 0};
        uint8_t args1[] = {0, 0, 255, 0, 0};

        simple_effect(BREATHE, pc_buf, frame + 16, times1, args1, colors + 6, 1, 1);
        simple_effect(BREATHE, gpu_buf, frame + 8, times1, args1, colors + 3, 2, 2);
        digital_effect(BREATHE, fan_buf, 12, 2, frame, times1, args1, colors, 2, 1);
    }
    else if((frame -= 128) < 128)
    {
        uint16_t times1[] = {16, 0, 16, 0, 0};
        uint16_t times2[] = {0, 32, 0, 32, 0};
        uint8_t args1[] = {0, 0, 255, 0, 0};
        uint8_t args2[5];
        args2[ARG_BIT_PACK] = SMOOTH;
        args2[ARG_FILL_PIECE_COUNT] = 2;
        args2[ARG_FILL_COLOR_COUNT] = 1;
        args2[ARG_FILL_PIECE_DIRECTIONS1] = 0;
        args2[ARG_FILL_PIECE_DIRECTIONS2] = 0;

        set_color(pc_buf, 0, 0, 0, 0);
        simple_effect(BREATHE, gpu_buf, frame + 16, times1, args1, colors, 2, 2);
        digital_effect(FILL, fan_buf, 12, 2, frame, times2, args2, colors + 3, 2, 1);
    }
    else if((frame -= 128) < 96)
    {
        uint16_t times1[] = {0, 16, 0, 16, 0};
        uint8_t args1[] = {0, 20, 255, 0, 0};

        simple_effect(BREATHE, pc_buf, frame + 16, times1, args1, colors + 6, 1, 1);
        simple_effect(BREATHE, gpu_buf, frame + 8, times1, args1, colors + 3, 2, 2);
        digital_effect(BREATHE, fan_buf, 12, 2, frame, times1, args1, colors, 2, 1);
    }
    else if((frame -= 96) < 128)
    {
        uint16_t times1[] = {16, 0, 16, 0, 0};
        uint16_t times2[] = {4, 30, 0, 30, 0};
        uint8_t args1[] = {0, 0, 255, 0, 0};
        uint8_t args2[5];
        args2[ARG_BIT_PACK] = SMOOTH;
        args2[ARG_FILL_PIECE_COUNT] = 1;
        args2[ARG_FILL_COLOR_COUNT] = 1;
        args2[ARG_FILL_PIECE_DIRECTIONS1] = 0;
        args2[ARG_FILL_PIECE_DIRECTIONS2] = 0;

        simple_effect(BREATHE, pc_buf, frame + 16, times1, args1, colors + 6, 1, 1);
        simple_effect(BREATHE, gpu_buf, frame + 16, times1, args1, colors + 3, 2, 2);
        digital_effect(FILL, fan_buf, 12, 2, frame, times2, args2, colors, 2, 1);
    }
}

#pragma clang diagnostic pop