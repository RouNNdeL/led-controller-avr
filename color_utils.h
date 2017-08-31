#ifndef LEDCONTROLLER_COLOR_UTILS_H
#define LEDCONTROLLER_COLOR_UTILS_H

typedef enum
{
    BREATHE, FADE
} effect;

uint8_t actual_brightness(uint8_t brightness);

void set_all_colors(uint8_t *p_buf, uint8_t r, uint8_t g, uint8_t b, uint8_t count);

void set_color(uint8_t *p_buf, uint8_t led, uint8_t r, uint8_t g, uint8_t b);

void simple_effect(effect effect, uint8_t *color, uint32_t frame, uint16_t *times, uint8_t *colors, uint8_t color_count);

#endif //LEDCONTROLLER_COLOR_UTILS_H