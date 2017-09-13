#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include "color_utils.h"
#include "eeprom.h"

#define LED_COUNT 12
#define FPS 64

extern void output_grb_strip(uint8_t *ptr, uint16_t count);

uint8_t *fan_buf;
uint8_t brightness;
uint8_t profile_count;
uint8_t n_profile;
profile current_profile;

volatile uint32_t frame = 0; /* 32 bits is enough for 2 years of continuous run at 64 fps */

void convert_bufs()
{
    /* Convert from RGB to GRB expected by WS2812B and convert to actual brightness*/
    for(uint8_t i = 0; i < LED_COUNT; ++i)
    {
        uint8_t index = i * 3;

        fan_buf[index] = actual_brightness(fan_buf[index]);
        fan_buf[index + 1] = actual_brightness(fan_buf[index + 1]);
        fan_buf[index + 2] = actual_brightness(fan_buf[index + 2]);

        uint8_t temp = fan_buf[index + 1];
        fan_buf[index + 1] = fan_buf[index];
        fan_buf[index] = temp;
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
uint16_t time_to_frames(uint8_t time)
{
    if(time <= 100)
    {
        return time * FPS / 8;
    }
    else if(time <= 180)
    {
        return time * FPS / 2 - FPS * 30;
    }
    else if(time < 240)
    {
        return (time - 120) * FPS;
    }
    else if(time < 254)
    {
        return (time * 30 - 7080) * FPS;
    }
    return 600 * FPS;
}
#pragma clang diagnostic pop

void update()
{
    output_grb_strip(fan_buf, sizeof(fan_buf));
}

void init_avr()
{
    DDRD |= (1 << PD2);
    TCCR1B |= (1 << WGM12);  /* Set timer1 to CTC mode */
    TIMSK1 |= (1 << OCIE1A); /* Enable timer1 clear interrupt */
    sei();                   /* Enable global interrupts */
    OCR1A = 977;             /* Set timer1 A register to reset every 1/64s */
    TCCR1B |= (1 << CS12);   /* Set the timer1 prescaler to 256 */
}

void init_eeprom()
{
    /* Fetch simple data from eeprom */
}

void fetch_profile(uint8_t n)
{
    /* Fetch profile from eeprom */
}

int main(void)
{
    init_avr();

    uint32_t previous_frame = 1;

    while(1)
    {
        if(previous_frame != frame)
        {
            previous_frame = frame;

            /* Calculate effects for all devices*/

            convert_bufs();
        }
    }
}

ISR(TIMER1_COMPA_vect)
{
    frame++;
    update();
}