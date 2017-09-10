#include <avr/io.h>
#include <avr/interrupt.h>
#include "color_utils.h"

#define LED_COUNT 12

extern void output_grb_strip(uint8_t *ptr, uint16_t count);

uint8_t strip_buf[LED_COUNT * 3];
volatile uint32_t frame = 0; /* 32 bits is enough for 2 years of continuous run at 64 fps */

void update()
{
    output_grb_strip(strip_buf, sizeof(strip_buf));
}

void init()
{
    DDRD |= (1 << PD2);
    TCCR1B |= (1 << WGM12);  /* Set timer1 to CTC mode */
    TIMSK1 |= (1 << OCIE1A); /* Enable timer1 clear interrupt */
    sei();                   /* Enable global interrupts */
    OCR1A = 977;             /* Set timer1 A register to reset every 1/64s */
    TCCR1B |= (1 << CS12);   /* Set the timer1 prescaler to 256 */
}

int main(void)
{
    init();

    uint8_t color[3];
    uint16_t times[] = {0, 64, 0, 0};
    uint8_t args[] = {1, 255, 0, 1};
    uint8_t colors[48];

    uint8_t brightness = 255;

    colors[0] = 0;
    colors[1] = brightness;
    colors[2] = 0;

    colors[3] = 0;
    colors[4] = 0;
    colors[5] = brightness;

    colors[6] = brightness;
    colors[7] = 0;
    colors[8] = 0;

    colors[9] = brightness;
    colors[10] = brightness;
    colors[11] = brightness;

    uint32_t previous_frame = 1;

    while(1)
    {
        if(previous_frame != frame)
        {
            previous_frame = frame;
            //simple_effect(FADE, color, frame, times, colors, 3);
            adv_effect(RAINBOW, strip_buf, LED_COUNT, 0, frame, times, args, colors, 1);
            //adv_effect(RAINBOW, strip_buf+18, LED_COUNT/2, 0, frame, times, args, colors, 1);
            //set_all_colors(strip_buf, color[0], color[1], color[2], LED_COUNT);
        }
    }
}

ISR(TIMER1_COMPA_vect)
{
    frame++;
    update();
}