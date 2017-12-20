#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include "color_utils.h"
#include "eeprom.h"
#include "uart.h"

#define FAN_LED_COUNT 12
#define STRIP_LED_COUNT 1
#define FPS 64

extern void output_grb_strip(uint8_t *ptr, uint16_t count);
extern void output_grb_fan(uint8_t * ptr, uint16_t count);

//TODO: Replace 3 with 12, as the ATmega1284P can fit more profiles then the ATmega328P
profile EEMEM profiles[3];
global_settings EEMEM globals_addr;

volatile uint8_t uart_control = 0x00;
uint8_t uart_free = 1; /* 0 if we are currently receiving data, 1 otherwise */
uint8_t fan_buf[FAN_LED_COUNT * 3];
uint8_t strip_buf[STRIP_LED_COUNT * 3];
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
    for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
    {
        uint16_t index = i * 3;

        fan_buf[index] = actual_brightness(fan_buf[index]);
        fan_buf[index + 1] = actual_brightness(fan_buf[index + 1]);
        fan_buf[index + 2] = actual_brightness(fan_buf[index + 2]);

        uint8_t temp = fan_buf[index + 1];
        fan_buf[index + 1] = fan_buf[index];
        fan_buf[index] = temp;
    }

    for(int i = 0; i < STRIP_LED_COUNT; ++i)
    {
        uint16_t index = i * 3;

        strip_buf[index] = actual_brightness(strip_buf[index]);
        strip_buf[index + 1] = actual_brightness(strip_buf[index + 1]);
        strip_buf[index + 2] = actual_brightness(strip_buf[index + 2]);

        uint8_t temp = strip_buf[index + 1];
        strip_buf[index + 1] = strip_buf[index];
        strip_buf[index] = temp;
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"

void output_analog1(uint8_t q1, uint8_t q2, uint8_t q3)
{
    OCR0B = q1;
    OCR1B = q2;
    OCR1A = q3;
}

void output_analog2(uint8_t q4, uint8_t q5, uint8_t q6)
{
    OCR2B = q4;
    OCR2A = q5;
    OCR0A = q6;
}

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
    frames[4] = time_to_frames(times[4]);
}

void update()
{
    output_grb_fan(fan_buf, sizeof(fan_buf));
    output_grb_strip(strip_buf, sizeof(strip_buf));

    /*if(frame % 64)
    {
        PINA &= ~(1 << PA3);
    }
    else
    {
        PINA |= (1 << PA3);
    }*/
}

void init_avr()
{
    /* Initialize Data Direction Registers */
    DDRA = 0x88; /* Pins PA3 and PA7 as output, the rest as input */
    DDRB = 0x18; /* Pins PB3 and PB4 as output, the rest as input */
    DDRC = 0x40; /* Pin PC6 as output, the rest as input */
    DDRD = 0xF4; /* Pis PD2 and PD4-PD7 as output, the rest as input */

    /* Initialize timer3 for time measurement */
    TCCR3B |= (1 << WGM32);  /* Set timer3 to CTC mode */
    TIMSK3 |= (1 << OCIE3A); /* Enable timer3 clear interrupt */
    OCR3A = 31250;           /* Set timer3 A register to reset every 1/64s */
    TCCR3B |= (1 << CS31);   /* Set the timer3 prescaler to 8 */

    /* Initialize timer0 for PWM */
    TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM00);
    TCCR0B = (1 << CS02);

    /* Initialize timer1 for PWM */
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
    TCCR1B = (1 << CS12);

    /* Initialize timer2 for PWM */
    TCCR2A = (1 << COM2A1) | (1 << COM2B1) | (1 << WGM20);
    TCCR2B = (1 << CS22);

    sei();                   /* Enable global interrupts */

    set_all_colors(strip_buf, 0, 0, 0, STRIP_LED_COUNT);
    set_all_colors(fan_buf, 0, 0, 0, FAN_LED_COUNT);
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

    while(1)
    {
        process_uart();

        if(previous_frame != frame)
        {
            previous_frame = frame;

            uint16_t times[] = {0, 0, 0, 64*5, 0};
            uint8_t args[5];
            args[ARG_BIT_PACK] = SMOOTH;
            args[ARG_RAINBOW_BRIGHTNESS] = 200;
            args[ARG_RAINBOW_SOURCES] = 1;
            uint8_t colors[48];

            //set_all_colors(fan_buf, color[0], color[1], color[2], FAN_LED_COUNT);
            //set_all_colors(fan_buf, 0, 0, 0, FAN_LED_COUNT);
            //digital_effect(RAINBOW, fan_buf, FAN_LED_COUNT, 2, frame, times, args , colors, 1);

            convert_bufs();
        }
    }
}

ISR(TIMER3_COMPA_vect)
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
