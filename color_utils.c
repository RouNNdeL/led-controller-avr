#include <avr/io.h>

uint8_t actual_brightness(uint8_t brightness)
{
    return (brightness * brightness) / 255;
}

void set_color(uint8_t *p_buf, uint8_t led, uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t index = 3 * led;
    p_buf[index++] = g;
    p_buf[index++] = r;
    p_buf[index] = b;
}

void set_all_colors(uint8_t *p_buf, uint8_t r, uint8_t g, uint8_t b, uint8_t count)
{
    for(uint8_t i = 0; i < count; ++i)
    {
        set_color(p_buf, i, r, g, b);
    }
}

uint32_t sum_times(uint16_t *times)
{
    return times[0] + times[1] + times[2] + times[3];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
/**
 *
 * @param color - an array of length 3 that the color is going to be assigned to
 * @param frame - a frame to calculate the color for
 * @param times - timing arguments resolved to frames
 * @param colors - an array of length 24 containing 16*4 colors
 *               in a proper configuration for set device (fans are always GRB)
 * @param color_count - how many colors are in use
 * @return - a color to be displayed at a given frame
 */
void breathing(uint8_t *color, uint32_t frame, uint16_t *times, uint8_t *colors, uint8_t color_count)
{
    uint32_t sum = sum_times(times);
    uint32_t d_time = frame % sum;
    uint32_t n_color = ((frame / sum) % color_count)*3;

    if(d_time < times[0])
    {
        color[0] = 0x00;
        color[1] = 0x00;
        color[2] = 0x00;
    }
    else if((d_time -= times[0]) < times[1])
    {
        float progress = (float) d_time / (float) times[1];
        color[0] = colors[n_color++] * progress;
        color[1] = colors[n_color++] * progress;
        color[2] = colors[n_color] * progress;
    }
    else if((d_time -= times[1]) < times[2])
    {
        color[0] = colors[n_color++];
        color[1] = colors[n_color++];
        color[2] = colors[n_color];
    }
    else if((d_time -= times[2]) < times[3])
    {
        float progress = 1-((float) d_time / (float) times[3]);
        color[0] = colors[n_color++] * progress;
        color[1] = colors[n_color++] * progress;
        color[2] = colors[n_color] * progress;
    }
}
#pragma clang diagnostic pop

