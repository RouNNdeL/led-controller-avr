#ifndef LEDCONTROLLER_COLOR_UTILS_H
#define LEDCONTROLLER_COLOR_UTILS_H

#define actual_brightness(brightness) (brightness * brightness) / 255;

typedef enum
{
    BREATHE, FADE, FILL
} effect;

void set_all_colors(uint8_t *p_buf, uint8_t r, uint8_t g, uint8_t b, uint8_t count);

void set_color(uint8_t *p_buf, uint8_t led, uint8_t r, uint8_t g, uint8_t b);

void simple_effect(effect effect, uint8_t *color, uint32_t frame, uint16_t *times, uint8_t *colors, uint8_t color_count);

void adv_effect(effect effect, uint8_t *leds, uint8_t count, uint8_t offset, uint32_t frame,
                uint16_t *times, uint8_t *colors, uint8_t color_count);

#endif //LEDCONTROLLER_COLOR_UTILS_H