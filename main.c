#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include <string.h>
#include "color_utils.h"
#include "eeprom.h"
#include "uart.h"
#include "config.h"
#include "csgo.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"

extern void output_grb_strip(uint8_t *ptr, uint16_t count);

extern void output_grb_fan(uint8_t *ptr, uint16_t count);

//<editor-fold desc="Flags">
#define FLAG_NEW_FRAME (1 << 0)
#define FLAG_BUTTON (1 << 1)
#define FLAG_RESET (1 << 2)

#if (COMPILE_CSGO != 0)
#define FLAG_CSGO_ENABLED (1 << 4)
#endif /* (COMPILE_CSGO != 0) */

#if (COMPILE_DEBUG != 0)
#define FLAG_DEBUG_ENABLED (1 << 5)

uint8_t debug_buffer[DEBUG_BUFFER_SIZE];
#endif /* (COMPILE_DEBUG != 0) */

volatile uint8_t flags = 0;
//</editor-fold>

//<editor-fold desc="IO">
#define PIN_RESET (1 << PD2)
#define PIN_BUTTON (1 << PA4)
#define PIN_LED (1 << PA3)
//</editor-fold>

//<editor-fold desc="Data">
device_profile EEMEM device_profiles[DEVICE_COUNT][DEVICE_PROFILE_COUNT];
global_settings EEMEM globals_addr;

global_settings globals = {
        255, 255, 255, 255, 255, 255,
        255, 255, 255,
        255, 0, 255,
        255, 0, 255,
        255, 0, 0,
        255, 0, 0,
        255, 128, 128,
        1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0,
        1, 0,
        2, 0,
        2, 2, 0
};
/* The first 3 bytes are the current color, the last 3 bytes are the old color to transition from */
uint8_t color_converted[6 * DEVICE_COUNT] = {};

volatile transition_t transition_frame[DEVICE_COUNT] = {0};
transition_t transition_frames[DEVICE_COUNT];

#define brightness(device, color) scale8(color, globals.brightness[device])
#define save_globals() eeprom_update_block(&globals, &globals_addr, GLOBALS_LENGTH)

#if (COMPILE_EFFECTS != 0)

device_profile current_profile[DEVICE_COUNT];
uint16_t frames[DEVICE_COUNT][TIME_COUNT];
uint8_t colors[DEVICE_COUNT][COLOR_COUNT * 3];
uint8_t backup_args[DEVICE_COUNT][ARG_COUNT];
uint32_t auto_increment;

#define fetch_profile(p, d, n) eeprom_read_block(&p, &device_profiles[d][n], DEVICE_LENGTH)
#define save_profile(p, d, n) eeprom_update_block(&p, &device_profiles[d][n], DEVICE_LENGTH)

#define increment_profile() globals.current_profile = (globals.current_profile+1)%globals.profile_count

#endif /* (COMPILE_EFFECTS != 0) */
//</editor-fold>

//<editor-fold desc="Uart">
#if (COMPILE_UART != 0)
volatile uint8_t uart_control = 0x00;
volatile uint8_t uart_buffer[99];
volatile uint8_t uart_buffer_length = 0;

#define UART_FLAG_RECEIVE (1 << 0)
#define UART_FLAG_LOCK (1 << 1)
uint8_t uart_flags = 0;

#define reset_uart() uart_buffer_length = 0;uart_flags &= ~UART_FLAG_RECEIVE;uart_control = 0x00
#endif /* (COMPILE_UART != 0) */
//</editor-fold>

//<editor-fold desc="Hardware">

uint8_t fan_buf[FAN_LED_COUNT * MAX_FAN_COUNT * 3];
uint8_t strip_buf_full[(STRIP_VIRTUAL_COUNT + 1) * 3];
uint8_t *strip_buf = strip_buf_full + 3;
uint8_t pc_buf[3];
uint8_t gpu_buf[3];

#define output_pc(color) output_analog1(color[0], color[1], color[2])
#define output_gpu(color) output_analog2(color[2], color[1], color[0])

#if (COMPILE_EFFECTS != 0)
#define simple(buf, n) simple_effect(current_profile[n].effect, buf, frame + frames[n][TIME_DELAY], frames[n],\
current_profile[n].args, colors[n], current_profile[n].color_count, 0)

#define digital(buf, count, offset, n) digital_effect(current_profile[n].effect, buf, count, offset, frame + frames[n][TIME_DELAY], frames[n],\
current_profile[n].args, colors[n], current_profile[n].color_count)
#endif /* (COMPILE_EFFECTS != 0) */
//</editor-fold>

#if (COMPILE_CSGO != 0)
game_state csgo_state = {0};
game_state old_csgo_state = {0};
csgo_control csgo_ctrl = {0};
#endif /* (COMPILE_CSGO != 0) */

volatile uint32_t frame = 0; /* 32 bits is enough for 2 years of continuous run at 64 fps */
#if (COMPILE_DEMOS != 0)
uint8_t demo = 0;
#endif /* (COMPILE_DEMOS != 0) */

#define all_enabled() all_any_en_dis(0, 1)
#define any_enabled() all_any_en_dis(1, 1)
#define all_disabled() all_any_en_dis(0, 0)
#define any_disabled() all_any_en_dis(1, 0)

uint8_t all_any_en_dis(uint8_t any, uint8_t enabled)
{
    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        if(globals.flags[i] & DEVICE_FLAG_ENABLED ^ enabled ^ any)
            return any;
    }
    return !any;
}

#if (COMPILE_EFFECTS != 0)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

void backup_all_args()
{
    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        memcpy(backup_args[i], current_profile[i].args, ARG_COUNT);
    }
}

void convert_simple_color_and_brightness()
{
    for(uint8_t d = 0; d < DEVICE_COUNT; ++d)
    {
        uint8_t index = d * 6;

        /* Copy the now old color to its place (3 bytes after the new) */
        memcpy(color_converted + index + 3, color_converted + index, 3);

        uint8_t brightness = (globals.flags[d] & DEVICE_FLAG_ENABLED) ? globals.brightness[d] : 0;
        set_color_manual(color_converted + index, color_brightness(brightness, color_from_buf(globals.color[d])));

        globals.flags[d] |= DEVICE_FLAG_TRANSITION;
    }
}

void convert_colors_for_brightness(uint8_t device)
{
    for(uint8_t j = 0; j < COLOR_COUNT; ++j)
    {
        uint8_t index = j * 3;
        set_color_manual(colors[device] + index,
                         color_brightness(globals.brightness[device],
                                          color_from_buf(current_profile[device].colors + index)));
    }

    if(current_profile[device].effect == RAINBOW)
    {
        current_profile[device].args[ARG_RAINBOW_BRIGHTNESS] =
                brightness(device, current_profile[device].args[ARG_RAINBOW_BRIGHTNESS]);
    }
}

#pragma clang diagnostic pop

void convert_all_colors()
{
    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        convert_colors_for_brightness(i);
    }
}

#endif /* (COMPILE_EFFECTS != 0) */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

void convert_bufs()
{
    /* Convert to actual brightness */

#if (ACTUAL_BRIGHTNESS_DIGITAL != 0)
    for(uint8_t i = 0; i < FAN_LED_COUNT * globals.fan_count; ++i)
    {
        uint8_t index = i * 3;

        fan_buf[index] = actual_brightness(fan_buf[index]);
        fan_buf[index + 1] = actual_brightness(fan_buf[index + 1]);
        fan_buf[index + 2] = actual_brightness(fan_buf[index + 2]);
    }
#endif /* (ACTUAL_BRIGHTNESS_DIGITAL != 0) */

#if (ACTUAL_BRIGHTNESS_DIGITAL != 0)
    for(uint8_t i = 0; i < STRIP_LED_COUNT + 1; ++i)
    {
        uint8_t index = i * 3;

        strip_buf_full[index] = actual_brightness(strip_buf_full[index]);
        strip_buf_full[index + 1] = actual_brightness(strip_buf_full[index + 1]);
        strip_buf_full[index + 2] = actual_brightness(strip_buf_full[index + 2]);
    }
#endif /* (ACTUAL_BRIGHTNESS_DIGITAL != 0) */
}

#pragma clang diagnostic pop

void set_disabled()
{
    if(!(globals.flags[DEVICE_INDEX_STRIP] & DEVICE_FLAG_ENABLED))
        set_all_colors(strip_buf, COLOR_BLACK, STRIP_LED_COUNT, 1);
    if(!(globals.flags[DEVICE_INDEX_PC] & DEVICE_FLAG_ENABLED))
        set_color(pc_buf, 0, COLOR_BLACK);
    if(!(globals.flags[DEVICE_INDEX_PC] & DEVICE_FLAG_ENABLED))
        set_color(gpu_buf, 0, COLOR_BLACK);
    for(uint8_t i = 0; i < MAX_FAN_COUNT; ++i)
    {
        if(!(globals.flags[DEVICE_INDEX_FAN(i)] & DEVICE_FLAG_ENABLED))
            set_all_colors(fan_buf + FAN_LED_COUNT * i * 3, COLOR_BLACK, FAN_LED_COUNT, 1);
    }
}

void output_analog1(uint8_t q1, uint8_t q2, uint8_t q3)
{
#if (ACTUAL_BRIGHTNESS_ANALOG1 != 0)
    OCR0B = actual_brightness(q1);
    OCR1B = actual_brightness(q2);
    OCR1A = actual_brightness(q3);
#else
    OCR0B = q1;
    OCR1B = q2;
    OCR1A = q3;
#endif /* (ACTUAL_BRIGHTNESS_ANALOG1 != 0) */
}

void output_analog2(uint8_t q4, uint8_t q5, uint8_t q6)
{
#if (ACTUAL_BRIGHTNESS_ANALOG2 != 0)
    OCR2B = actual_brightness(q4);
    OCR2A = actual_brightness(q5);
    OCR0A = actual_brightness(q6);
#else
    OCR2B = q4;
    OCR2A = q5;
    OCR0A = q6;
#endif /* (ACTUAL_BRIGHTNESS_ANALOG2 != 0) */
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"

uint16_t time_to_frames(uint8_t time)
{
    if(time <= 80)
    {
        return time * FPS / 16;
    }
    if(time <= 120)
    {
        return time * FPS / 8 - 5 * FPS;
    }
    if(time <= 160)
    {
        return time * FPS / 2 - 50 * FPS;
    }
    if(time <= 190)
    {
        return (time - 130) * FPS;
    }
    if(time <= 235)
    {
        return (2 * time - 320) * FPS;
    }
    if(time <= 245)
    {
        return (15 * time - 3375) * FPS;
    }
    return (60 * time - 14400) * FPS;
}

#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"

uint32_t autoincrement_to_frames(uint8_t time)
{
    if(time <= 60)
    {
        return time * FPS / 2;
    }
    if(time <= 90)
    {
        return (time - 30) * FPS;
    }
    if(time <= 126)
    {
        return (5 * time / 2 - 165) * FPS;
    }
    if(time <= 156)
    {
        return (5 * time - 480) * FPS;
    }
    if(time <= 196)
    {
        return (15 * time - 2040) * FPS;
    }
    if(time <= 211)
    {
        return (60 * time - 10860) * FPS;
    }
    if(time <= 253)
    {
        return (300 * time - 61500) * FPS;
    }
    if(time == 254) return 18000 * FPS;
    return 21600 * FPS;
}

#pragma clang diagnostic pop

void convert_to_frames(uint16_t *frames, uint8_t *times)
{
    for(uint8_t i = 0; i < TIME_COUNT; ++i)
    {
        frames[i] = time_to_frames(times[i]);
    }
}

#if (COMPILE_EFFECTS != 0)

void convert_all_frames()
{
    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
    {
        convert_to_frames(frames[i], current_profile[i].timing);
    }
}

void load_devices()
{
    for(uint8_t d = 0; d < DEVICE_COUNT; d++)
    {
        fetch_profile(current_profile[d], d, globals.current_device_profile[d]);
    }
    convert_all_frames();
    backup_all_args();
    convert_all_colors();
}

void load_profile(uint8_t n)
{
    for(uint8_t d = 0; d < DEVICE_COUNT; d++)
    {
        if(globals.profiles[n][d] > -1)
        {
            fetch_profile(current_profile[d], d, globals.profiles[n][d]);
            globals.current_device_profile[d] = globals.profiles[n][d];
        }
    }
    globals.profile_flags[globals.current_profile] = globals.profile_flags[n];
    save_globals();
}

void refresh_profile()
{
    load_profile(globals.current_profile);
    convert_all_frames();
    backup_all_args();
    convert_all_colors();
}

#endif /* (COMPILE_EFFECTS !=0) */

void update()
{
    output_grb_fan(fan_buf, FAN_LED_COUNT * 3 * globals.fan_count);
    output_grb_strip(strip_buf_full, STRIP_LED_COUNT * 3 + 3);

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

    PORTA = 0x00;
    PORTB = 0x00;
    PORTC = 0x00;
    PORTD = 0x00;

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

    set_all_colors(strip_buf_full, COLOR_BLACK, STRIP_LED_COUNT + 1, 1);
    set_all_colors(fan_buf, COLOR_BLACK, FAN_LED_COUNT, 1);

    set_color(pc_buf, 0, COLOR_BLACK);
    set_color(gpu_buf, 0, COLOR_BLACK);
}


void init_eeprom()
{
    eeprom_read_block(&globals, &globals_addr, GLOBALS_LENGTH);
#if (COMPILE_EFFECTS != 0)
    auto_increment = autoincrement_to_frames(globals.auto_increment);

    load_devices();
#endif /* (COMPILE_EFFECTS !=0) */

    convert_simple_color_and_brightness();
    for(int d = 0; d < DEVICE_COUNT; ++d)
    {
        memcpy(color_converted + d * 6 + 3, color_converted + d * 6, 3);
        globals.flags[d] &= ~DEVICE_FLAG_TRANSITION;
        transition_frame[d] = 0;
    }
}

#if (COMPILE_UART != 0)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

void process_uart()
{
    if(uart_control)
    {
        switch(uart_control)
        {
#if  (COMPILE_EFFECTS != 0)
            case SAVE_PROFILE:
            {
                //<editor-fold desc="Save profile">
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= DEVICE_LENGTH + 2)
                {
                    /*
                     * uart_buffer[0] - device_profile index
                     * uart_buffer[1] - device index
                     */
                    if(uart_buffer[0] < DEVICE_PROFILE_COUNT)
                    {
                        /*
                         * Do not immediately write to EEPROM if the profile being modified is the current one,
                         * instead write to the variable itself and set an update flag.
                         */
                        if(globals.current_device_profile[uart_buffer[1]] == uart_buffer[0])
                        {
                            /* Lock the buffer before reading it */
                            uart_flags |= UART_FLAG_LOCK;
                            memcpy(&(current_profile[uart_buffer[1]]), (const void *) (uart_buffer + 2),
                                   DEVICE_LENGTH);
                            uart_flags &= ~UART_FLAG_LOCK;
                            convert_to_frames(frames[uart_buffer[1]], current_profile[uart_buffer[1]].timing);
                            memcpy(backup_args[uart_buffer[1]], current_profile[uart_buffer[1]].args, 5);
                            convert_colors_for_brightness(uart_buffer[1]);

                            globals.flags[uart_buffer[1]] |= DEVICE_FLAG_PROFILE_UPDATED;
                        }
                        else
                        {
                            device_profile received;

                            /* Lock the buffer before reading it */
                            uart_flags |= UART_FLAG_LOCK;
                            memcpy(&received, (const void *) (uart_buffer + 2), DEVICE_LENGTH);
                            uart_flags &= ~UART_FLAG_LOCK;

                            save_profile(received, uart_buffer[1], uart_buffer[0]);
                        }
                        uart_transmit(RECEIVE_SUCCESS);
                    }
                    else
                    {
                        uart_transmit(PROFILE_OVERFLOW);
                    }

#if (COMPILE_DEBUG != 0)
                    flags &= ~FLAG_DEBUG_ENABLED;
#endif /* (COMPILE_DEBUG != 0) */
                    reset_uart();
                }
                //</editor-fold>
                break;
            }
            case SAVE_GLOBALS:
            {
                //<editor-fold desc="Save globals">
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= GLOBALS_LENGTH)
                {
                    /* Lock the buffer before reading it */
                    uint8_t previous_profile = globals.current_profile;
                    uart_flags |= UART_FLAG_LOCK;
                    memcpy(&globals, (const void *) (uart_buffer), GLOBALS_LENGTH);
                    uart_flags &= ~UART_FLAG_LOCK;

                    save_globals();
                    if(previous_profile != globals.current_profile)
                    {
                        refresh_profile();
                        frame = 0;
                    }
                    auto_increment = autoincrement_to_frames(globals.auto_increment);
                    convert_all_colors();
                    convert_simple_color_and_brightness();

                    uart_transmit(RECEIVE_SUCCESS);

#if (COMPILE_DEBUG != 0)
                    flags &= ~FLAG_DEBUG_ENABLED;
#endif /* (COMPILE_DEBUG != 0) */
                    reset_uart();
                }
                //</editor-fold>
                break;
            }
            case SAVE_PROFILE_FLAGS:
            {
                //<editor-fold desc="Save profile flags">
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= 2)
                {
                    globals.profile_flags[uart_buffer[0]] = uart_buffer[1];
                    save_globals();

                    uart_transmit(RECEIVE_SUCCESS);

#if (COMPILE_DEBUG != 0)
                    flags &= ~FLAG_DEBUG_ENABLED;
#endif /* (COMPILE_DEBUG != 0) */
                    reset_uart();
                }
                //</editor-fold>
                break;
            }
            case SEND_PROFILE:
            {
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= 2)
                {
                    /*
                     * uart_buffer[0] - device_profile index
                     * uart_buffer[1] - device index
                     */
                    if(uart_buffer[0] < DEVICE_PROFILE_COUNT)
                    {
                        /* Lock the buffer before reading it */
                        profile transmit;
                        fetch_profile(transmit, uart_buffer[1], uart_buffer[0]);
                        transmit_bytes((uint8_t *) &transmit, PROFILE_LENGTH);
                    }
                    else
                    {
                        uart_transmit(PROFILE_OVERFLOW);
                    }

                    reset_uart();
                }
                break;
            }
            case SEND_GLOBALS:
            {
                transmit_bytes((uint8_t *) &globals, GLOBALS_LENGTH);
                uart_buffer_length = 0;
                uart_control = 0x00;
                break;
            }
            case SAVE_EXPLICIT:
            {
                for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
                {
                    if(globals.flags[i] & DEVICE_FLAG_PROFILE_UPDATED)
                        save_profile(current_profile[i], i, globals.current_device_profile[i]);
                    globals.flags[i] &= ~DEVICE_FLAG_PROFILE_UPDATED;

                }
                uart_transmit(RECEIVE_SUCCESS);

                reset_uart();
                break;
            }
            case TEMP_DEVICE:
            {
                //<editor-fold desc="Temporary device (for Google Assistant)">
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= DEVICE_LENGTH + 2)
                {
                    /* Lock the buffer before reading it */
                    uart_flags |= UART_FLAG_LOCK;
                    memcpy(&(current_profile[uart_buffer[1]]), (const void *) (uart_buffer + 2), DEVICE_LENGTH);
                    uart_flags &= ~UART_FLAG_LOCK;

                    convert_all_frames();

                    uart_transmit(RECEIVE_SUCCESS);

                    reset_uart();
                }
                break;
                //</editor-fold>
            }
            case FRAME_JUMP:
            case DEBUG_SET_FRAME:
            {
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= sizeof(uint32_t))
                {
                    uint32_t new_frame;
                    uart_flags |= UART_FLAG_LOCK;
                    memcpy(&new_frame, (const void *) (uart_buffer), sizeof(uint32_t));
                    uart_flags &= ~UART_FLAG_LOCK;

                    frame = new_frame;
                    uart_transmit(RECEIVE_SUCCESS);

                    reset_uart();
                }
                break;
            }
#else
            case SAVE_PROFILE:
            case SAVE_GLOBALS:
            case SEND_GLOBALS:
            case SEND_PROFILE:
            case SAVE_EXPLICIT:
            case TEMP_DEVICE:
            {
                uart_transmit(EFFECTS_DISABLED);

                reset_uart();
                break;
            }
#endif /* (COMPILE_EFFECTS !=0) */
#if (COMPILE_DEBUG != 0)
            case DEBUG_START:
            {
                flags |= FLAG_DEBUG_ENABLED;
                flags |= FLAG_NEW_FRAME;
                uart_transmit(RECEIVE_SUCCESS);

                reset_uart();
                break;
            }
            case DEBUG_STOP:
            {
                flags &= ~FLAG_DEBUG_ENABLED;
                uart_transmit(RECEIVE_SUCCESS);

                reset_uart();
                break;
            }
            case DEBUG_GET_FRAME:
            {
                transmit_bytes((uint8_t *) &frame, sizeof(frame));

                reset_uart();
                break;
            }
            case DEBUG_INCREMENT_FRAME:
            {
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= sizeof(int32_t))
                {
                    int32_t increment;
                    uart_flags |= UART_FLAG_LOCK;
                    memcpy(&increment, (const void *) (uart_buffer), sizeof(increment));
                    uart_flags &= ~UART_FLAG_LOCK;

                    if(increment < 0 && frame < -increment)
                    {
                        frame = 0;
                    }
                    else
                    {
                        frame += increment;
                    }
                    flags |= FLAG_NEW_FRAME;
                    uart_transmit(RECEIVE_SUCCESS);

                    reset_uart();
                }
                break;
            }
            case DEBUG_SEND_INFO:
            {
                transmit_bytes((uint8_t *) &frame, sizeof(frame));
                uart_transmit(flags);
                transmit_bytes(debug_buffer, sizeof(debug_buffer));
                uart_buffer_length = 0;
                uart_control = 0x00;
                break;
            }
#else
            case DEBUG_START:
            case DEBUG_STOP:
            case DEBUG_GET_FRAME:
            case DEBUG_INCREMENT_FRAME:
            {
                uart_transmit(DEBUG_DISABLED);

                reset_uart();
                break;
            }
#endif /* (COMPILE_DEBUG != 0) */
            case DEMO_START_MUSIC:
            case DEMO_START_EFFECTS:
            {
#if (COMPILE_DEMOS != 0)
                demo = uart_control;
                frame = 0;
                reset_uart();
#else
                uart_transmit(DEMOS_DISABLED);
                reset_uart();
#endif /* (COMPILE_DEMOS != 0) */
                break;
            }
#if (COMPILE_CSGO != 0)
            case CSGO_BEGIN:
            {
                flags |= FLAG_CSGO_ENABLED;
                uart_transmit(RECEIVE_SUCCESS);

                reset_uart();
                break;
            }
            case CSGO_END:
            {
                flags &= ~FLAG_CSGO_ENABLED;
                uart_transmit(RECEIVE_SUCCESS);

                reset_uart();
                break;
            }
            case CSGO_NEW_STATE:
            {
                if(!(uart_flags & UART_FLAG_RECEIVE))
                {
                    uart_transmit(READY_TO_RECEIVE);
                    uart_flags |= UART_FLAG_RECEIVE;
                }
                else if(uart_buffer_length >= CSGO_STATE_LENGTH)
                {
                    game_state new_state;
                    uart_flags |= UART_FLAG_LOCK;
                    memcpy(&new_state, (const void *) (uart_buffer), CSGO_STATE_LENGTH);
                    uart_flags &= ~UART_FLAG_LOCK;

                    //<editor-fold desc="Reset frames for new data">
                    if(new_state.ammo != csgo_state.ammo)
                    {
                        old_csgo_state.ammo = csgo_state.ammo;
                        csgo_state.ammo = new_state.ammo;
                        csgo_ctrl.ammo_frame = 0;
                    }
                    if(new_state.health != csgo_state.health)
                    {
                        old_csgo_state.health = csgo_state.health;
                        csgo_state.health = new_state.health;
                        csgo_ctrl.health_frame = 0;
                        csgo_ctrl.damage_transition_frame = 0;
                    }
                    if(new_state.ammo != csgo_state.ammo)
                    {
                        old_csgo_state.ammo = csgo_state.ammo;
                        csgo_state.ammo = new_state.ammo;
                        csgo_ctrl.ammo_frame = 0;
                    }
                    if(new_state.flashed != csgo_state.flashed)
                    {
                        old_csgo_state.flashed = csgo_state.flashed;
                        csgo_state.flashed = new_state.flashed;
                        csgo_ctrl.flash_frame = 0;
                    }
                    if(new_state.round_state != csgo_state.round_state)
                    {
                        old_csgo_state.round_state = csgo_state.round_state;
                        csgo_state.round_state = new_state.round_state;
                        csgo_ctrl.round_state_frame = 0;
                    }
                    if(new_state.weapon_slot != csgo_state.weapon_slot)
                    {
                        old_csgo_state.weapon_slot = csgo_state.weapon_slot;
                        csgo_state.weapon_slot = new_state.weapon_slot;
                        csgo_ctrl.ammo_frame = 0;
                        csgo_ctrl.bomb_slot_frame = 0;
                    }
                    if(new_state.bomb_state != csgo_state.bomb_state)
                    {
                        old_csgo_state.bomb_state = csgo_state.bomb_state;
                        csgo_state.bomb_state = new_state.bomb_state;
                        csgo_ctrl.bomb_overall_frame = 0;
                        csgo_ctrl.bomb_frame = 0;
                        csgo_ctrl.bomb_tick_rate = BOMB_TICK_INIT;
                    }
                    //</editor-fold>

                    if(!csgo_ctrl.damage && old_csgo_state.health > csgo_state.health)
                    {
                        csgo_ctrl.damage_frame = 0;
                    }
                    if(old_csgo_state.health > csgo_state.health)
                    {
                        csgo_ctrl.damage_buffer_frame = 0;
                    }

                    csgo_ctrl.damage_previous = csgo_ctrl.damage;
                    csgo_ctrl.damage += (old_csgo_state.health > csgo_state.health ?
                                         old_csgo_state.health - csgo_state.health : 0);

                    if(csgo_state.health == 0)
                    {
                        csgo_ctrl.damage =
                                csgo_ctrl.damage > DAMAGE_MIN_ON_DEATH ? csgo_ctrl.damage : DAMAGE_MIN_ON_DEATH;
                    }

                    uart_transmit(RECEIVE_SUCCESS);

                    reset_uart();
                }
                break;
            }
#else
            case CSGO_BEGIN:
            case CSGO_NEW_STATE:
            case CSGO_END:
            {
                uart_transmit(CSGO_DISABLED);
            }
#endif /* (COMPILE_CSGO != 0) */
            default:
            {
                uart_transmit(UNRECOGNIZED_COMMAND);
                reset_uart();
            }
        }
    }
}

#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ImplicitIntegerAndEnumConversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif /* (COMPILE_UART != 0) */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"

int main(void)
{
    init_avr();
#if (COMPILE_UART != 0)
    init_uart();
#endif /* (COMPILE_UART != 0) */

#if (COMPILE_BUTTONS != 0)
    uint32_t button_frame = 0;
    uint32_t reset_frame = 0;
#endif /* (COMPILE_BUTTONS != 0) */
    init_eeprom();

    while(1)
    {
        //<editor-fold desc="Buttons">
#if (COMPILE_BUTTONS != 0)
        if((PINA & PIN_BUTTON) && !(flags & FLAG_BUTTON))
        {
            button_frame = frame;
#if (COMPILE_DEBUG != 0)
            flags &= ~FLAG_DEBUG_ENABLED;
#endif /* (COMPILE_DEBUG != 0) */
            flags |= FLAG_BUTTON;
        }
        else if(button_frame && !(PINA & PIN_BUTTON))
        {
            uint32_t time = frame - button_frame;
            if(time > BUTTON_MIN_FRAMES)
            {
#if (COMPILE_EFFECTS != 0)
                for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
                {
                    if(globals.flags[i] & DEVICE_FLAG_PROFILE_UPDATED)
                        save_profile(current_profile[i], i, globals.current_device_profile[i]);
                }
#endif /* (COMPILE_EFFECTS != 0) */

                if(time < BUTTON_OFF_FRAMES)
                {
                    if(any_enabled())
                    {
                        save_globals();
#if (COMPILE_EFFECTS != 0)
                        increment_profile();
                        refresh_profile();
#endif /* (COMPILE_EFFECTS != 0) */
                        frame = 0;
                    }
                    else
                    {
                        for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
                        {
                            globals.flags[i] |= DEVICE_FLAG_ENABLED;
                        }
                    }
#if (COMPILE_UART != 0)
                    uart_transmit(GLOBALS_UPDATED);
#endif /* (COMPILE_UART != 0) */
                }
                else if(time < BUTTON_RESET_FRAMES)
                {
                    for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
                    {
                        globals.flags[i] &= ~DEVICE_FLAG_ENABLED;
                    }
                    save_globals();
#if (COMPILE_UART != 0)
                    uart_transmit(GLOBALS_UPDATED);
#endif /* (COMPILE_UART != 0) */
                    frame = 0;
                }
                else
                {
                    PORTD |= PIN_RESET;
                    flags |= FLAG_RESET;
                    reset_frame = frame + 32;
                }
            }

            button_frame = 0;
            flags &= ~FLAG_BUTTON;
        }
#endif /* (COMPILE_BUTTONS != 0) */
        //</editor-fold>

#if (COMPILE_UART != 0)
        process_uart();
#endif /* (COMPILE_UART != 0) */

        if(flags & FLAG_NEW_FRAME)
        {

#if (COMPILE_EFFECTS != 0)
            if(auto_increment && frame && frame % auto_increment == 0 && all_enabled())
            {
                for(uint8_t i = 0; i < DEVICE_COUNT; ++i)
                {
                    if(globals.flags[i] & DEVICE_FLAG_PROFILE_UPDATED)
                        save_profile(current_profile[i], i, globals.current_device_profile[i]);
                    globals.flags[i] &= ~DEVICE_FLAG_PROFILE_UPDATED;
                }
                increment_profile();
                refresh_profile();
                uart_transmit(GLOBALS_UPDATED);
                frame = 0;
            }
#endif /* (COMPILE_EFFECTS != 0) */

#if (COMPILE_DEMOS != 0)
            if(demo)
            {
                uint8_t demo_finished = 1;
                switch(demo)
                {
                    case DEMO_START_MUSIC:
                        demo_finished = demo_music(fan_buf, pc_buf, gpu_buf, frame);
                        break;
                    case DEMO_START_EFFECTS:
                        demo_finished = demo_effects(fan_buf, pc_buf, gpu_buf, frame);
                        break;
                }


                convert_bufs();
                if(demo_finished)
                {
                    demo = 0;
                    frame = 0;
                    uart_transmit(DEMO_END);
                }
            }
            else
            {
#endif /* (COMPILE_DEMOS != 0) */

#if (COMPILE_CSGO != 0)
            if(flags & FLAG_CSGO_ENABLED)
            {
                process_csgo(&csgo_ctrl, &csgo_state, &old_csgo_state, fan_buf, globals.fan_config[0], gpu_buf,
                             pc_buf,
                             strip_buf);

                csgo_increment_frames();
                set_disabled();
                convert_bufs();
            }
            else
            {
#endif /* (COMPILE_CSGO != 0) */

                uint8_t simple_color[DEVICE_COUNT][3];
                for(uint8_t d = 0; d < DEVICE_COUNT; ++d)
                {
                    uint8_t index = d * 6;
                    cross_fade(simple_color[d], color_converted + index, 3, 0,
                               transition_frame[d] * UINT8_MAX / TRANSITION_QUICK_FRAMES);
                    if(transition_frame[d] >= TRANSITION_QUICK_FRAMES && globals.flags[d] & DEVICE_FLAG_TRANSITION)
                    {
                        memcpy(color_converted + index + 3, color_converted + index, 3);
                        globals.flags[d] &= ~DEVICE_FLAG_TRANSITION;
                        transition_frame[d] = 0;
                    }
                }

                if(globals.brightness[DEVICE_INDEX_PC] && globals.flags[DEVICE_INDEX_PC] & DEVICE_FLAG_ENABLED &&
                   globals.flags[DEVICE_INDEX_PC] & DEVICE_FLAG_EFFECT_ENABLED)
                {
#if (COMPILE_EFFECTS != 0)
                    simple(pc_buf, DEVICE_INDEX_PC);
#endif
                }
                else if(globals.brightness[DEVICE_INDEX_PC] && (globals.flags[DEVICE_INDEX_PC] & DEVICE_FLAG_ENABLED ||
                                                                globals.flags[DEVICE_INDEX_PC] &
                                                                DEVICE_FLAG_TRANSITION))
                    set_color(pc_buf, 0, color_from_buf(simple_color[DEVICE_INDEX_PC]));
                else
                    set_color(pc_buf, 0, COLOR_BLACK);

                if(globals.brightness[DEVICE_INDEX_GPU] && globals.flags[DEVICE_INDEX_GPU] & DEVICE_FLAG_ENABLED &&
                   globals.flags[DEVICE_INDEX_GPU] & DEVICE_FLAG_EFFECT_ENABLED)
                {
#if (COMPILE_EFFECTS != 0)
                    simple(gpu_buf, DEVICE_INDEX_GPU);
#endif
                }
                else if(globals.brightness[DEVICE_INDEX_GPU] &&
                        (globals.flags[DEVICE_INDEX_GPU] & DEVICE_FLAG_ENABLED ||
                         globals.flags[DEVICE_INDEX_GPU] & DEVICE_FLAG_TRANSITION))
                    set_color(gpu_buf, 0, color_from_buf(simple_color[DEVICE_INDEX_GPU]));
                else
                    set_color(gpu_buf, 0, COLOR_BLACK);

                for(uint8_t f = 0; f < globals.fan_count; ++f)
                {
                    uint8_t *index = fan_buf + FAN_LED_COUNT * f * 3;
                    if(globals.brightness[DEVICE_INDEX_FAN(f)] &&
                       globals.flags[DEVICE_INDEX_FAN(f)] & DEVICE_FLAG_ENABLED &&
                       globals.flags[DEVICE_INDEX_FAN(f)] & DEVICE_FLAG_EFFECT_ENABLED)
                    {
#if (COMPILE_EFFECTS != 0)
                        digital(index, FAN_LED_COUNT, globals.fan_config[f], DEVICE_INDEX_FAN(f));
#endif
                    }
                    else if(globals.brightness[DEVICE_INDEX_FAN(f)] &&
                            (globals.flags[DEVICE_INDEX_FAN(f)] & DEVICE_FLAG_ENABLED ||
                             globals.flags[DEVICE_INDEX_FAN(f)] & DEVICE_FLAG_TRANSITION))
                    {
                        set_all_colors(index, color_from_buf(simple_color[DEVICE_INDEX_FAN(f)]), FAN_LED_COUNT, 1);
                    }
                    else
                    {
                        set_all_colors(index, COLOR_BLACK, FAN_LED_COUNT, 1);
                    }
                }

                //<editor-fold desc="Strip calculations and transformations">
                if(globals.profile_flags[globals.current_profile] & PROFILE_FLAG_FRONT_PC)
                {
                    //<editor-fold desc="Front as PC">
                    for(uint8_t i = 0; i < STRIP_FRONT_LED_COUNT; ++i)
                    {
                        uint8_t index = i * 3;
                        strip_buf[STRIP_SIDE_LED_COUNT * 6 + index++] = pc_buf[1];
                        strip_buf[STRIP_SIDE_LED_COUNT * 6 + index++] = pc_buf[0];
                        strip_buf[STRIP_SIDE_LED_COUNT * 6 + index] = pc_buf[2];
                    }
                    if(globals.brightness[DEVICE_INDEX_STRIP] &&
                       globals.flags[DEVICE_INDEX_STRIP] & DEVICE_FLAG_ENABLED &&
                       globals.flags[DEVICE_INDEX_STRIP] & DEVICE_FLAG_EFFECT_ENABLED)
                    {
#if (COMPILE_EFFECTS != 0)
                        if(globals.profile_flags[globals.current_profile] & PROFILE_FLAG_STRIP_MODE)
                        {

                            digital(strip_buf, STRIP_SIDE_LED_COUNT * 2, 0, DEVICE_INDEX_STRIP);
                        }
                        else
                        {
                            digital(strip_buf, STRIP_SIDE_LED_COUNT, 0, DEVICE_INDEX_STRIP);
                            for(uint8_t i = 0; i < STRIP_SIDE_LED_COUNT; ++i)
                            {
                                uint8_t index = i * 3;
                                strip_buf[STRIP_SIDE_LED_COUNT * 6 - index - 3] = strip_buf[index];
                                strip_buf[STRIP_SIDE_LED_COUNT * 6 - index - 2] = strip_buf[index + 1];
                                strip_buf[STRIP_SIDE_LED_COUNT * 6 - index - 1] = strip_buf[index + 2];
                            }
                        }
#endif
                    }
                    else if(globals.brightness[DEVICE_INDEX_STRIP] &&
                            (globals.flags[DEVICE_INDEX_STRIP] & DEVICE_FLAG_ENABLED ||
                             globals.flags[DEVICE_INDEX_STRIP] & DEVICE_FLAG_TRANSITION))
                    {
                        set_all_colors(strip_buf, color_from_buf(simple_color[DEVICE_INDEX_STRIP]),
                                       STRIP_LED_COUNT, 1);
                    }
                    else
                    {
                        set_all_colors(strip_buf, COLOR_BLACK, STRIP_LED_COUNT, 1);
                    }
                    //</editor-fold>
                }
                else if(globals.brightness[DEVICE_INDEX_STRIP] &&
                        globals.flags[DEVICE_INDEX_STRIP] & DEVICE_FLAG_ENABLED &&
                        globals.flags[DEVICE_INDEX_STRIP] & DEVICE_FLAG_EFFECT_ENABLED)
                {
#if (COMPILE_EFFECTS != 0)
                    if(globals.profile_flags[globals.current_profile] & PROFILE_FLAG_STRIP_MODE)
                    {
                        //<editor-fold desc="Loop">
                        digital(strip_buf, STRIP_VIRTUAL_COUNT, 0, DEVICE_INDEX_STRIP);

                        uint8_t front_virtual[STRIP_VIRTUAL_COUNT * 3];
                        /* This shifts the second side of the strip forward by STRIP_FRONT_LED_COUNT */
                        memcpy(front_virtual, strip_buf + STRIP_SIDE_LED_COUNT * 3, STRIP_VIRTUAL_COUNT * 3);
                        memmove(strip_buf + STRIP_SIDE_LED_COUNT * 3,
                                strip_buf + (STRIP_SIDE_LED_COUNT + STRIP_FRONT_VIRTUAL_COUNT) * 3,
                                STRIP_SIDE_LED_COUNT * 3);

                        uint8_t front[STRIP_FRONT_LED_COUNT * 3];
                        for(uint8_t i = 0; i < STRIP_FRONT_LED_COUNT / 2; ++i)
                        {
                            uint8_t index = i * 3;
                            set_color_manual(front + index, color_from_buf(front_virtual + index * 2));
                        }

                        uint8_t front_half = STRIP_FRONT_LED_COUNT / 2 * 3;
                        front[front_half] = (front_virtual[front_half * 2] + front_virtual[front_half * 2 - 3]) / 2;
                        front[front_half + 1] =
                                (front_virtual[front_half * 2 + 1] + front_virtual[front_half * 2 - 2]) / 2;
                        front[front_half + 2] =
                                (front_virtual[front_half * 2 + 2] + front_virtual[front_half * 2 - 1]) / 2;

                        for(uint8_t i = STRIP_FRONT_LED_COUNT / 2 + 1; i < STRIP_FRONT_LED_COUNT; ++i)
                        {
                            uint8_t index = i * 3;
                            set_color_manual(front + index, color_from_buf(front_virtual + index * 2 - 3));
                        }

                        uint8_t i = STRIP_FRONT_LED_COUNT - 1;
                        uint8_t j = 0;
                        while(i > j)
                        {
                            uint8_t index = i * 3;
                            uint8_t index2 = j * 3;
                            uint8_t r = front[index];
                            uint8_t g = front[index + 1];
                            uint8_t b = front[index + 2];
                            front[index] = front[index2];
                            front[index + 1] = front[index2 + 1];
                            front[index + 2] = front[index2 + 2];
                            front[index2] = r;
                            front[index2 + 1] = g;
                            front[index2 + 2] = b;
                            i--;
                            j++;
                        }

                        memcpy(strip_buf + STRIP_SIDE_LED_COUNT * 6, front, STRIP_FRONT_LED_COUNT * 3);
                        //</editor-fold>
                    }
                    else
                    {
                        //<editor-fold desc="Single strip">
                        if(globals.profile_flags[globals.current_profile] & PROFILE_FLAG_FRONT_MODE)
                        {
                            digital(strip_buf, STRIP_SIDE_LED_COUNT, 0, DEVICE_INDEX_STRIP);
                            set_all_colors(strip_buf + STRIP_SIDE_LED_COUNT * 6,
                                           color_from_buf(strip_buf + STRIP_SIDE_LED_COUNT * 3 - 3),
                                           STRIP_FRONT_LED_COUNT, 0);
                        }
                        else
                        {
                            uint8_t front_half = (STRIP_FRONT_LED_COUNT + 1) / 2;
                            digital(strip_buf, STRIP_SIDE_LED_COUNT + front_half, 0, DEVICE_INDEX_STRIP);
                            for(uint8_t i = 0; i < front_half; ++i)
                            {
                                uint8_t index = i * 3;
                                strip_buf[STRIP_SIDE_LED_COUNT * 6 + index] =
                                        strip_buf[STRIP_SIDE_LED_COUNT * 3 + index];
                                strip_buf[STRIP_SIDE_LED_COUNT * 6 + index + 1] =
                                        strip_buf[STRIP_SIDE_LED_COUNT * 3 + index + 1];
                                strip_buf[STRIP_SIDE_LED_COUNT * 6 + index + 2] =
                                        strip_buf[STRIP_SIDE_LED_COUNT * 3 + index + 2];

                                strip_buf[STRIP_SIDE_LED_COUNT * 6 + STRIP_FRONT_LED_COUNT * 3 - index - 3] =
                                        strip_buf[STRIP_SIDE_LED_COUNT * 3 + index];
                                strip_buf[STRIP_SIDE_LED_COUNT * 6 + STRIP_FRONT_LED_COUNT * 3 - index - 2] =
                                        strip_buf[STRIP_SIDE_LED_COUNT * 3 + index + 1];
                                strip_buf[STRIP_SIDE_LED_COUNT * 6 + STRIP_FRONT_LED_COUNT * 3 - index - 1] =
                                        strip_buf[STRIP_SIDE_LED_COUNT * 3 + index + 2];
                            }
                        }
                        for(uint8_t i = 0; i < STRIP_SIDE_LED_COUNT; ++i)
                        {
                            uint8_t index = i * 3;
                            strip_buf[STRIP_SIDE_LED_COUNT * 6 - index - 3] = strip_buf[index];
                            strip_buf[STRIP_SIDE_LED_COUNT * 6 - index - 2] = strip_buf[index + 1];
                            strip_buf[STRIP_SIDE_LED_COUNT * 6 - index - 1] = strip_buf[index + 2];
                        }
                        //</editor-fold>
                    }
#endif
                }
                else if(globals.brightness[DEVICE_INDEX_STRIP] &&
                        (globals.flags[DEVICE_INDEX_STRIP] & DEVICE_FLAG_ENABLED ||
                         globals.flags[DEVICE_INDEX_STRIP] & DEVICE_FLAG_TRANSITION))
                {
                    set_all_colors(strip_buf, color_from_buf(simple_color[DEVICE_INDEX_STRIP]),
                                   STRIP_LED_COUNT, 1);
                }
                else
                {
                    set_all_colors(strip_buf, COLOR_BLACK, STRIP_LED_COUNT, 1);
                }
                //</editor-fold>

                convert_bufs();
            }
#if (COMPILE_CSGO != 0)
        }
#endif /* (COMPILE_CSGO != 0) */
#if (COMPILE_DEMOS != 0)
        }
#endif /* (COMPILE_DEMOS != 0) */

#if (COMPILE_BUTTONS != 0)
        if((flags & FLAG_RESET) && frame > reset_frame)
        {
            PORTD &= ~PIN_RESET;
            flags &= ~FLAG_RESET;
        }
#endif /* (COMPILE_BUTTONS != 0) */
#if (COMPILE_DEBUG != 0)
        if(!(uart_flags & UART_FLAG_RECEIVE) && (flags & FLAG_DEBUG_ENABLED || !(frame % (FPS / 2))))
        {
            uart_transmit(DEBUG_NEW_INFO);
        }
#endif /* (COMPILE_DEBUG != 0) */

        flags &= ~FLAG_NEW_FRAME;
        update();
    }
}

#pragma clang diagnostic pop

#pragma clang diagnostic pop

ISR(TIMER3_COMPA_vect)
{
#if (COMPILE_DEBUG != 0)
    if(!(flags & FLAG_DEBUG_ENABLED))
    {
#endif /* (COMPILE_DEBUG != 0) */
    frame++;
    flags |= FLAG_NEW_FRAME;
    for(int d = 0; d < DEVICE_COUNT; ++d)
    {
        if(globals.flags[d] & DEVICE_FLAG_TRANSITION)
        {
            transition_frame[d]++;
        }
    }
#if (COMPILE_DEBUG != 0)
    }
#endif /* (COMPILE_DEBUG != 0) */
}

#if (COMPILE_UART != 0)

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

#endif /* (COMPILE_UART != 0) */

#pragma clang diagnostic pop