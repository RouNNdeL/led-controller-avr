#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include "color_utils.h"
#include "eeprom.h"
#include "uart.h"

#define LED_COUNT 12
#define FPS 64

extern void output_grb_strip(uint8_t *ptr, uint16_t count);

//TODO: Replace 3 with 12, as the ATmega1284P can fit more profiles then the ATmega328P
profile EEMEM profiles[3];
global_settings EEMEM globals_addr;

uint8_t fan_buf[LED_COUNT * 3];
global_settings globals;
profile current_profile;

volatile uint32_t frame = 1; /* 32 bits is enough for 2 years of continuous run at 64 fps */

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
    else if(time <= 240)
    {
        return (time - 120) * FPS;
    }
    else if(time <= 254)
    {
        return (time * 30 - 7080) * FPS;
    }
    return 600 * FPS;
}

#pragma clang diagnostic pop

void convert_to_frames(uint16_t *frames, uint8_t *times)
{
    frames[0] = time_to_frames(times[0]);
    frames[1] = time_to_frames(times[1]);
    frames[2] = time_to_frames(times[2]);
    frames[3] = time_to_frames(times[3]);
}

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

void fetch_profile(uint8_t n)
{
    eeprom_read_block(&current_profile, &profiles[n], sizeof(profile));
}

void save_profile(void *p, uint8_t n)
{
    eeprom_update_block(p, &profiles[n], sizeof(profile));
}

void save_status()
{
    eeprom_update_block(&globals, &globals_addr, sizeof(global_settings));
}

void init_eeprom()
{
    eeprom_read_block(&globals, &globals_addr, sizeof(global_settings));

    if(globals.leds_enabled)
    {
        fetch_profile(globals.n_profile);
    }
}

int main(void)
{
    init_avr();

    uint32_t previous_frame = 0;

    init_eeprom();

    while(1)
    {
        if(previous_frame != frame)
        {
            previous_frame = frame;

            device_profile device = current_profile.devices[0];
            if(frame % 320 == 0)
            {
                globals.n_profile = (globals.n_profile+1)%3;
                save_status();
                fetch_profile(globals.n_profile);
            }
            uint16_t timing_frames[4];
            convert_to_frames(timing_frames, device.timing);
            adv_effect((effect) device.mode, fan_buf, LED_COUNT, 0, frame, timing_frames, device.args, device.colors,
                       device.color_count);

            convert_bufs();
        }
    }
}

ISR(TIMER1_COMPA_vect)
{
    frame++;
    update();
}
