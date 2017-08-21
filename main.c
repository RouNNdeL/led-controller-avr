#include <avr/io.h>
#include <util/delay.h>
#include "eeprom.h"

#define PIN 0
#define POUT 1

struct device_profile
{
    uint8_t mode;
    uint8_t color_count;
    uint8_t timing[4];
    uint8_t args[4];
    uint8_t colors[16 * 3];
};

struct profile
{
    uint8_t name[30];
    struct device_profile devices[5];
};

extern void output_grb_fan(uint8_t *ptr, uint16_t count);

extern void output_grb_strip(uint8_t *ptr, uint16_t count);

void set_color(uint8_t *p_buf, uint8_t led, uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t index = 3 * led;
    p_buf[index++] = g;
    p_buf[index++] = r;
    p_buf[index] = b;
}

void init()
{
    //PC2 and PC5 set to output for digital LEDs, PC3 set to input for profile switch
    DDRC |= (POUT << PC2) | (POUT << PC5) & ~(PIN << PC3);
    TCCR0A |= (1 << WGM00) | (1 << WGM01);
}

int main(void)
{
    uint8_t led_count = 12;
    uint8_t fan[led_count * 3];
    while(1)
    {
        _delay_ms(1000);
    }
}