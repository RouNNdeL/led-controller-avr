#ifndef LEDCONTROLLER_COLOR_UTILS_H
#define LEDCONTROLLER_COLOR_UTILS_H

uint8_t actual_brightness(uint8_t brightness);

void breathing(uint8_t *color, uint32_t frame, uint16_t *times, uint8_t *colors, uint8_t color_count);

void fading(uint8_t *color, uint32_t frame, uint16_t *times, uint8_t *colors, uint8_t color_count);

void set_all_colors(uint8_t *p_buf, uint8_t r, uint8_t g, uint8_t b, uint8_t count);

void set_color(uint8_t *p_buf, uint8_t led, uint8_t r, uint8_t g, uint8_t b);

#endif //LEDCONTROLLER_COLOR_UTILS_H