#ifndef LEDCONTROLLER_COLOR_UTILS_H
#define LEDCONTROLLER_COLOR_UTILS_H

#define actual_brightness(brightness) (brightness * brightness) / 255;
#define DIRECTION_BIT 0
#define SMOOTH_BIT 1

#define DIRECTION (1 << DIRECTION_BIT)
#define SMOOTH (1 << SMOOTH_BIT)

typedef enum
{
    BREATHE = 0x00,
    FADE = 0x01,
    RAINBOW = 0x02,
    FILL = 0x03,
    ROTATING = 0x05,
    PIECES = 0x0C,

} effect;

void set_all_colors(uint8_t *p_buf, uint8_t r, uint8_t g, uint8_t b, uint8_t count);

void set_color(uint8_t *p_buf, uint8_t led, uint8_t r, uint8_t g, uint8_t b);

void simple_effect(effect effect, uint8_t *color, uint32_t frame, uint16_t *times, uint8_t *colors, uint8_t color_count);

void adv_effect(effect effect, uint8_t *leds, uint8_t led_count, uint8_t offset, uint32_t frame,
                uint16_t *times, uint8_t *args, uint8_t *colors, uint8_t color_count);

#endif //LEDCONTROLLER_COLOR_UTILS_H