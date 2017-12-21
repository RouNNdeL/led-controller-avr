#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include <string.h>
#include "color_utils.h"
#include "eeprom.h"
#include "uart.h"

#define FAN_LED_COUNT 12
#define MAX_FAN_COUNT 3
#define STRIP_LED_COUNT 0

#define FPS 64

#define BUTTON_MIN_FRAMES 2
#define BUTTON_OFF_FRAMES 64
#define BUTTON_RESET_FRAMES 320

#define FLAG_BUTTON (1 << 0)
#define FLAG_RESET (1 << 1)

#define PIN_RESET (1 << PD2)
#define PIN_BUTTON (1 << PA4)
#define PIN_LED (1 << PA3)

extern void output_grb_strip(uint8_t *ptr, uint16_t count);
extern void output_grb_fan(uint8_t *ptr, uint16_t count);

profile EEMEM profiles[8];
global_settings EEMEM globals_addr;

global_settings globals;
profile current_profile;
uint16_t frames[6][6];

volatile uint8_t uart_control = 0x00;
volatile uint8_t uart_buffer[64];
volatile uint8_t uart_buffer_length = 0;

#define UART_FLAG_RECEIVE (1 << 0)
#define UART_FLAG_LOCK (1 << 1)
uint8_t uart_flags = 0;

uint8_t fan_buf[FAN_LED_COUNT * MAX_FAN_COUNT * 3];
uint8_t strip_buf[(STRIP_LED_COUNT+1) * 3];
uint8_t pc_buf[3];
uint8_t gpu_buf[3];

volatile uint32_t frame = 0; /* 32 bits is enough for 2 years of continuous run at 64 fps */
volatile uint8_t new_frame = 1;

#define fetch_profile(p, n) eeprom_read_block(&p, &profiles[n], PROFILE_LENGTH)
#define change_profile(n) eeprom_read_block(&current_profile, &profiles[n], PROFILE_LENGTH)
#define save_profile(p, n) eeprom_update_block(&p, &profiles[n], PROFILE_LENGTH)
#define save_globals() eeprom_update_block(&globals, &globals_addr, GLOBALS_LENGTH)

#define output_pc(color) output_analog1(color[0], color[1], color[2])
#define output_gpu(color) output_analog2(color[2], color[1], color[0])

#define brightness(color) (actual_brightness(color)) * globals.brightness / UINT8_MAX
#define increment_profile() globals.n_profile %= globals.n_profile+1; save_globals(); change_profile(globals.n_profile);convert_all_frames();

void convert_bufs()
{
    /* Convert from RGB to GRB expected by WS2812B and convert to actual brightness */
    for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
    {
        uint16_t index = i * 3;

        fan_buf[index] = brightness(fan_buf[index]);
        fan_buf[index + 1] = brightness(fan_buf[index + 1]);
        fan_buf[index + 2] = brightness(fan_buf[index + 2]);

        uint8_t temp = fan_buf[index + 1];
        fan_buf[index + 1] = fan_buf[index];
        fan_buf[index] = temp;
    }

    for(int i = 0; i < STRIP_LED_COUNT; ++i)
    {
        uint16_t index = i * 3;

        strip_buf[index] = brightness(strip_buf[index]);
        strip_buf[index + 1] = brightness(strip_buf[index + 1]);
        strip_buf[index + 2] = brightness(strip_buf[index + 2]);

        uint8_t temp = strip_buf[index + 1];
        strip_buf[index + 1] = strip_buf[index];
        strip_buf[index] = temp;
    }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"

void output_analog1(uint8_t q1, uint8_t q2, uint8_t q3)
{
    OCR0B = brightness(q1);
    OCR1B = brightness(q2);
    OCR1A = brightness(q3);
}

void output_analog2(uint8_t q4, uint8_t q5, uint8_t q6)
{
    OCR2B = brightness(q4);
    OCR2A = brightness(q5);
    OCR0A = brightness(q6);
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
    frames[5] = time_to_frames(times[5]);
}

void convert_all_frames()
{
    for(uint8_t i = 0; i < 6; ++i)
    {
        convert_to_frames(frames[i], current_profile.devices[i].timing);
    }
}

void update()
{
    output_grb_fan(fan_buf, sizeof(fan_buf));
    output_grb_strip(strip_buf, sizeof(strip_buf));

    output_pc(pc_buf);
    output_gpu(gpu_buf);
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

    set_all_colors(strip_buf, 0, 0, 0, STRIP_LED_COUNT+1);
    set_all_colors(fan_buf, 0, 0, 0, FAN_LED_COUNT);
}

void init_eeprom()
{
    eeprom_read_block(&globals, &globals_addr, GLOBALS_LENGTH);

    change_profile(globals.n_profile);
}

void process_uart()
{
    if(uart_control)
    {
        switch(uart_control)
        {
            case SAVE_PROFILE:
            {
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= DEVICE_LENGTH + 2)
                {
                    profile received;
                    fetch_profile(received, uart_buffer[0]);

                    /* Lock the buffer before reading it */
                    uart_flags |= UART_FLAG_LOCK;
                    memcpy(&(received.devices[uart_buffer[1]]), (const void *) (uart_buffer + 2), DEVICE_LENGTH);
                    uart_flags &= ~UART_FLAG_LOCK;

                    save_profile(received, uart_buffer[0]);

                    uart_transmit(RECEIVE_SUCCESS);

                    uart_buffer_length = 0;
                    uart_flags &= ~UART_FLAG_RECEIVE;
                    uart_control = 0x00;
                }
                break;
            }
            case SAVE_GLOBALS:
            {
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= GLOBALS_LENGTH)
                {
                    /* Lock the buffer before reading it */
                    uart_flags |= UART_FLAG_LOCK;
                    memcpy(&globals, (const void *) (uart_buffer), GLOBALS_LENGTH);
                    uart_flags &= ~UART_FLAG_LOCK;

                    save_globals();

                    uart_transmit(RECEIVE_SUCCESS);

                    uart_buffer_length = 0;
                    uart_flags &= ~UART_FLAG_RECEIVE;
                    uart_control = 0x00;
                }
                break;
            }
            default:
            {
                uart_transmit(UNRECOGNIZED_COMMAND);
            }
        }
    }
}

int main(void)
{
    init_avr();
    init_uart();

    uint32_t button_frame = 0;
    uint32_t reset_frame = 0;
    uint8_t flags = 0;

    init_eeprom();

    while(1)
    {
        //TODO: Fix a bug that causes the button to completely stop working after pressing it a couple times
        if((PINA & PIN_BUTTON) && !(flags & FLAG_BUTTON))
        {
            button_frame = frame;
            flags |= FLAG_BUTTON;
        }
        else if(button_frame && !(PINA & PIN_BUTTON))
        {
            uint32_t time = frame - button_frame;
            if(time < BUTTON_OFF_FRAMES && globals.leds_enabled)
            {
                //TODO: Change to the next profile, send serial event 'profile_n';
                increment_profile();
                frame = 0;
            }
            else if(time < BUTTON_RESET_FRAMES)
            {
                //TODO: Turn off/on the LEDs, send serial event 'leds_state'
                globals.leds_enabled = !globals.leds_enabled;
                save_globals();
                frame = 0;
            }
            else
            {
                PORTD |= PIN_RESET;
                flags |= FLAG_RESET;
                reset_frame = frame + 32;
            }

            button_frame = 0;
            flags &= ~FLAG_BUTTON;
        }

        process_uart();

        if(new_frame)
        {
            new_frame = 0;

            if(globals.leds_enabled)
            {
                device_profile pc = current_profile.devices[DEVICE_PC];
                simple_effect(pc.effect, pc_buf, frame+frames[DEVICE_PC][5], frames[DEVICE_PC],
                              pc.args, pc.colors, pc.color_count, pc.color_cycles);
                device_profile gpu = current_profile.devices[DEVICE_GPU];
                simple_effect(gpu.effect, gpu_buf, frame+frames[DEVICE_GPU][5], frames[DEVICE_GPU],
                              gpu.args, gpu.colors, gpu.color_count, gpu.color_cycles);

                for(uint8_t i = 0; i < globals.fan_count; ++i)
                {
                    device_profile fan = current_profile.devices[DEVICE_FAN+i];
                    digital_effect(fan.effect, fan_buf+FAN_LED_COUNT*i, FAN_LED_COUNT, globals.fan_config[i], frame,
                                   frames[DEVICE_FAN+i], fan.args, fan.colors, fan.color_count, fan.color_cycles);
                }

                /* Enable when the strip is installed */
                /*device_profile strip = current_profile.devices[DEVICE_STRIP];
                digital_effect(strip.effect, strip_buf+3, STRIP_LED_COUNT, 0, frame, frames[DEVICE_STRIP],
                               strip.args, strip.colors, strip.color_count, strip.color_cycles);*/
            }
            else
            {
                set_all_colors(fan_buf, 0, 0, 0, FAN_LED_COUNT);
                set_all_colors(strip_buf, 0, 0, 0, STRIP_LED_COUNT+1);

                set_color(pc_buf, 0, 0, 0, 0);
                set_color(gpu_buf, 0, 0, 0, 0);
            }

            convert_bufs();

            if((flags & FLAG_RESET) && frame > reset_frame)
            {
                PORTD &= ~PIN_RESET;
                flags &= ~FLAG_RESET;
            }
        }
    }
}

ISR(TIMER3_COMPA_vect)
{
    frame++;
    new_frame = 1;
    update();
}

ISR(USART0_RX_vect)
{
    uint8_t val = UDR0;
    if(!(uart_flags & UART_FLAG_RECEIVE))
    {
        uart_control = val;
    }
    else if(uart_buffer_length < sizeof(uart_buffer) && !(uart_flags & UART_FLAG_LOCK))
    {
        uart_buffer[uart_buffer_length] = val;
        uart_buffer_length++;
    }
    else
    {
        uart_transmit(BUFFER_OVERFLOW);
    }
}
