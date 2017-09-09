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
    uint16_t times[] = {32, 32, 32, 32};
    uint8_t colors[48];

    uint8_t brightness = 10;
    colors[0] = brightness;
    colors[1] = 0;
    colors[2] = 0;

    colors[3] = brightness;
    colors[4] = brightness;
    colors[5] = 0;

    colors[6] = 0;
    colors[7] = brightness;
    colors[8] = 0;

    colors[9] = 0;
    colors[10] = brightness;
    colors[11] = brightness;

    colors[12] = 0;
    colors[13] = 0;
    colors[14] = brightness;

    colors[15] = brightness;
    colors[16] = 0;
    colors[17] = brightness;

    uint32_t previous_frame = 1;

    while(1)
    {
        if(previous_frame != frame)
        {
            previous_frame = frame;
            //simple_effect(BREATHE, color, frame, times, colors, 4);
            adv_effect(FILL, strip_buf, LED_COUNT, 0, frame, times, colors, 6);
            //set_all_colors(strip_buf, actual_brightness(color[0]), actual_brightness(color[1]), actual_brightness(color[2]), LED_COUNT);
        }
    }
}

ISR(TIMER1_COMPA_vect)
{
    frame++;
    update();
}