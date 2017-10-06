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

volatile uint8_t uart_control = 0x00;
uint8_t uart_free = 1; /* 0 if we are currently receiving data, 1 otherwise */
uint8_t fan_buf[LED_COUNT * 3];
global_settings globals;
profile current_profile;

volatile uint32_t frame = 1; /* 32 bits is enough for 2 years of continuous run at 64 fps */


#define fetch_profile(n) eeprom_read_block(&current_profile, &profiles[n], PROFILE_LENGTH);
#define get_profile(p, n) eeprom_read_block(&p, &profiles[n], PROFILE_LENGTH);
#define save_profile(p, n)eeprom_update_block(p, &profiles[n], PROFILE_LENGTH);
#define save_globals() eeprom_update_block(&globals, &globals_addr, GLOBALS_LENGTH);

void convert_bufs()
{
    /* Convert from RGB to GRB expected by WS2812B and convert to actual brightness */
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
    OCR1A = 31250;           /* Set timer1 A register to reset every 1/64s */
    TCCR1B |= (1 << CS11);   /* Set the timer1 prescaler to 8 */
}

void init_eeprom()
{
    eeprom_read_block(&globals, &globals_addr, GLOBALS_LENGTH);

    if(globals.leds_enabled)
    {
        fetch_profile(globals.n_profile);
    }
}

void process_uart()
{
    if(uart_control)
    {
        switch(uart_control)
        {
            case SAVE_PROFILE:
            {
                uart_free = 0;
                uint8_t n = uart_receive();            /* Get profile's index */
                profile receive;
                receive_bytes((uint8_t *) &receive, PROFILE_LENGTH); /* Receive the whole 320-byte profile */
                uart_free = 1;
                save_profile(&receive, n);             /* Save the received profile to eeprom */
                break;
            }
            case SAVE_GLOBALS:
            {
                uart_free = 0;
                receive_bytes((uint8_t *) &globals, GLOBALS_LENGTH);
                uart_free = 1;
                save_globals();
                break;
            }
            case SEND_GLOBALS:
            {
                uart_free = 0;
                transmit_bytes((uint8_t *) &globals, GLOBALS_LENGTH);
                uart_free = 1;
                break;
            }
            case SEND_PROFILE:
            {
                uart_free = 0;
                uint8_t n = uart_receive();            /* Get profile's index */
                profile transmit;
                get_profile(transmit, n);
                transmit_bytes((uint8_t *) &transmit, PROFILE_LENGTH);
                uart_free = 1;
                break;
            }
            default:
            {
                uart_transmit(UNRECOGNIZED_COMMAND);
            }
        }
        uart_control = 0x00;
    }
}

int main(void)
{
    init_avr();
    init_uart();

    uint32_t previous_frame = 0;

    init_eeprom();

    uint8_t leds = 0;

    while(1)
    {
        process_uart();

        if(previous_frame != frame)
        {
            previous_frame = frame;

            if(frame % 16 == 0)
            {
                leds = (leds + 1) % (LED_COUNT / 2);
            }
            uint16_t times[] = {64, 64, 64, 64, 0};
            uint8_t args[5];
            args[ARG_BIT_PACK] = DIRECTION | SMOOTH | RAINBOW_MODE;
            args[ARG_RAINBOW_SOURCES] = 1;
            args[ARG_RAINBOW_BRIGHTNESS] = 255;
            uint8_t colors[48];
            uint8_t b = 255;
            set_color(colors, 0, b, 0, 0);
            set_color(colors, 1, 0, b, 0);
            set_color(colors, 2, 0, 0, b);
            set_color(colors, 3, b, 0, 0);

            uint8_t color[3];
            digital_effect(BREATHE, fan_buf, LED_COUNT, 0, frame, times, args, colors, 3);

            //set_all_colors(fan_buf, color[0], color[1], color[2], LED_COUNT);
            //set_all_colors(fan_buf, 0, 0, 0, LED_COUNT);
            //digital_effect(RAINBOW, fan_buf, LED_COUNT, 2, frame, times, args , colors, 1);

            convert_bufs();
        }
    }
}

ISR(TIMER1_COMPA_vect)
{
    frame++;
    update();
}

ISR(USART_RX_vect)
{
    uint8_t val = UDR0;
    if(uart_free)
    {
        uart_control = val;
    }
}
