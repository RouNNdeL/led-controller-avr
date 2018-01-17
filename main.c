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

extern void output_grb_strip(uint8_t *ptr, uint16_t count);

extern void output_grb_fan(uint8_t *ptr, uint16_t count);

//<editor-fold desc="Flags">
#define FLAG_NEW_FRAME (1 << 0)
#define FLAG_BUTTON (1 << 1)
#define FLAG_RESET (1 << 2)
#define FLAG_PROFILE_UPDATED (1 << 3)

#if (COMPILE_CSGO != 0)
#define FLAG_CSGO_ENABLED (1 << 4)
#endif /* (COMPILE_CSGO != 0) */

volatile uint8_t flags = 0;
//</editor-fold>

//<editor-fold desc="IO">
#define PIN_RESET (1 << PD2)
#define PIN_BUTTON (1 << PA4)
#define PIN_LED (1 << PA3)
//</editor-fold>

//<editor-fold desc="Data">
#if (COMPILE_EFFECTS != 0)
profile EEMEM profiles[PROFILE_COUNT];
global_settings EEMEM globals_addr;

global_settings globals;
profile current_profile;
uint16_t frames[6][6];
uint16_t auto_increment;

#define fetch_profile(p, n) eeprom_read_block(&p, &profiles[n], PROFILE_LENGTH)
#define change_profile(n) eeprom_read_block(&current_profile, &profiles[n], PROFILE_LENGTH)
#define save_profile(p, n) eeprom_update_block(&p, &profiles[n], PROFILE_LENGTH)
#define save_globals() eeprom_update_block(&globals, &globals_addr, GLOBALS_LENGTH)

#define increment_profile() globals.n_profile = (globals.n_profile+1)%globals.profile_count
#define refresh_profile() change_profile(globals.profile_order[globals.n_profile]); convert_all_frames()

#define brightness(color) (color * globals.brightness) / UINT8_MAX
#endif /* (COMPILE_EFFECTS != 0) */
//</editor-fold>

//<editor-fold desc="Uart">
#if (COMPILE_UART != 0)
volatile uint8_t uart_control = 0x00;
volatile uint8_t uart_buffer[64];
volatile uint8_t uart_buffer_length = 0;

#define UART_FLAG_RECEIVE (1 << 0)
#define UART_FLAG_LOCK (1 << 1)
uint8_t uart_flags = 0;

#define reset_uart() uart_buffer_length = 0;uart_flags &= ~UART_FLAG_RECEIVE;uart_control = 0x00
#endif /* (COMPILE_UART != 0) */
//</editor-fold>

//<editor-fold desc="Hardware">

uint8_t fan_buf[FAN_LED_COUNT * MAX_FAN_COUNT * 3];
uint8_t strip_buf[(STRIP_LED_COUNT + 1) * 3];
uint8_t pc_buf[3];
uint8_t gpu_buf[3];

#define output_pc(color) output_analog1(color[0], color[1], color[2])
#define output_gpu(color) output_analog2(color[2], color[1], color[0])

#define simple(buf, n) simple_effect(current_profile.devices[n].effect, buf, frame + frames[n][TIME_DELAY], frames[n],\
current_profile.devices[n].args, current_profile.devices[n].colors, current_profile.devices[n].color_count, current_profile.devices[n].color_cycles)

#define digital(buf, count, offset, n) digital_effect(current_profile.devices[n].effect, buf, count, offset, frame + frames[n][TIME_DELAY], frames[n],\
current_profile.devices[n].args, current_profile.devices[n].colors, current_profile.devices[n].color_count, current_profile.devices[n].color_cycles)
//</editor-fold>

#if (COMPILE_CSGO != 0)
game_state csgo_state = {0, 0, 0, 0, 0};
game_state old_csgo_state = {0, 0, 0, 0, 0};
csgo_control csgo_ctrl = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif /* (COMPILE_CSGO != 0) */

volatile uint32_t frame = 0; /* 32 bits is enough for 2 years of continuous run at 64 fps */
#if (COMPILE_DEMOS != 0)
uint8_t demo = 0;
#endif /* (COMPILE_DEMOS != 0) */

void convert_bufs()
{
    /* Convert from RGB to GRB expected by WS2812B and convert to actual brightness */
    for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
    {
        uint8_t index = i * 3;

#if (ACTUAL_BRIGHTNESS_DIGITAL != 0)
        fan_buf[index] = actual_brightness(fan_buf[index]);
        fan_buf[index + 1] = actual_brightness(fan_buf[index + 1]);
        fan_buf[index + 2] = actual_brightness(fan_buf[index + 2]);
#endif /* #if (ACTUAL_BRIGHTNESS_DIGITAL != 0) */

        uint8_t temp = fan_buf[index + 1];
        fan_buf[index + 1] = fan_buf[index];
        fan_buf[index] = temp;
    }

    for(uint8_t i = 0; i < STRIP_LED_COUNT; ++i)
    {
        uint16_t index = i * 3;

#if (ACTUAL_BRIGHTNESS_DIGITAL != 0)
        strip_buf[index] = actual_brightness(strip_buf[index]);
        strip_buf[index + 1] = actual_brightness(strip_buf[index + 1]);
        strip_buf[index + 2] = actual_brightness(strip_buf[index + 2]);
#endif /* #if (ACTUAL_BRIGHTNESS_DIGITAL != 0) */

        uint8_t temp = strip_buf[index + 1];
        strip_buf[index + 1] = strip_buf[index];
        strip_buf[index] = temp;
    }
}

#if (COMPILE_EFFECTS !=0)
void apply_brightness()
{
    for(uint8_t i = 0; i < FAN_LED_COUNT; ++i)
    {
        uint8_t index = i * 3;

        fan_buf[index] = brightness(fan_buf[index]);
        fan_buf[index + 1] = brightness(fan_buf[index + 1]);
        fan_buf[index + 2] = brightness(fan_buf[index + 2]);
    }

    for(uint8_t i = 0; i < STRIP_LED_COUNT; ++i)
    {
        uint16_t index = i * 3;

        strip_buf[index] = brightness(strip_buf[index]);
        strip_buf[index + 1] = brightness(strip_buf[index + 1]);
        strip_buf[index + 2] = brightness(strip_buf[index + 2]);
    }

    pc_buf[0] = brightness(pc_buf[0]);
    pc_buf[1] = brightness(pc_buf[1]);
    pc_buf[2] = brightness(pc_buf[2]);

    gpu_buf[0] = brightness(gpu_buf[0]);
    gpu_buf[1] = brightness(gpu_buf[1]);
    gpu_buf[2] = brightness(gpu_buf[2]);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif /* (COMPILE_EFFECTS !=0) */

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

void convert_to_frames(uint16_t *frames, uint8_t *times)
{
    frames[0] = time_to_frames(times[0]);
    frames[1] = time_to_frames(times[1]);
    frames[2] = time_to_frames(times[2]);
    frames[3] = time_to_frames(times[3]);
    frames[4] = time_to_frames(times[4]);
    frames[5] = time_to_frames(times[5]);
}

#if (COMPILE_EFFECTS !=0)
void convert_all_frames()
{
    for(uint8_t i = 0; i < 6; ++i)
    {
        convert_to_frames(frames[i], current_profile.devices[i].timing);
    }
}
#endif /* (COMPILE_EFFECTS !=0) */

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

    set_all_colors(strip_buf, 0, 0, 0, STRIP_LED_COUNT + 1);
    set_all_colors(fan_buf, 0, 0, 0, FAN_LED_COUNT);

    set_color(pc_buf, 0, 0, 0, 0);
    set_color(gpu_buf, 0, 0, 0, 0);
}

#if (COMPILE_EFFECTS !=0)
void init_eeprom()
{
    eeprom_read_block(&globals, &globals_addr, GLOBALS_LENGTH);
    auto_increment = time_to_frames(globals.auto_increment) * AUTO_INCREMENT_MULTIPLIER;

    change_profile(globals.n_profile);
    convert_all_frames();
}
#endif /* (COMPILE_EFFECTS !=0) */

#if (COMPILE_UART != 0)
void process_uart()
{
    if(uart_control)
    {
        switch(uart_control)
        {
#if  (COMPILE_EFFECTS !=0)
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
                    if(uart_buffer[0] < PROFILE_COUNT)
                    {
                        /*
                         * Do not immediately write to EEPROM if the profile being modified is the current one,
                         * instead write to the variable itself and set an update flag.
                         */
                        if(globals.profile_order[globals.n_profile] == uart_buffer[0])
                        {
                            /* Lock the buffer before reading it */
                            uart_flags |= UART_FLAG_LOCK;
                            memcpy(&(current_profile.devices[uart_buffer[1]]), (const void *) (uart_buffer + 2),
                                   DEVICE_LENGTH);
                            uart_flags &= ~UART_FLAG_LOCK;
                            convert_to_frames(frames[uart_buffer[1]], current_profile.devices[uart_buffer[1]].timing);

                            flags |= FLAG_PROFILE_UPDATED;
                        }
                        else
                        {
                            profile received;
                            fetch_profile(received, uart_buffer[0]);

                            /* Lock the buffer before reading it */
                            uart_flags |= UART_FLAG_LOCK;
                            memcpy(&(received.devices[uart_buffer[1]]), (const void *) (uart_buffer + 2),
                                   DEVICE_LENGTH);
                            uart_flags &= ~UART_FLAG_LOCK;

                            save_profile(received, uart_buffer[0]);
                        }
                        uart_transmit(RECEIVE_SUCCESS);
                    }
                    else
                    {
                        uart_transmit(PROFILE_OVERFLOW);
                    }


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
                    uint8_t previous_profile = globals.n_profile;
                    uint8_t previous_profile_index = globals.profile_order[globals.n_profile];
                    uint8_t previous_enabled = globals.leds_enabled;
                    uart_flags |= UART_FLAG_LOCK;
                    memcpy(&globals, (const void *) (uart_buffer), GLOBALS_LENGTH);
                    uart_flags &= ~UART_FLAG_LOCK;

                    save_globals();
                    if(previous_profile != globals.n_profile || previous_enabled != globals.leds_enabled
                       || previous_profile_index != globals.profile_order[globals.n_profile])
                    {
                        refresh_profile();
                        frame = 0;
                    }
                    auto_increment = time_to_frames(globals.auto_increment) * AUTO_INCREMENT_MULTIPLIER;

                    uart_transmit(RECEIVE_SUCCESS);

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
                else if(uart_buffer_length >= 1)
                {
                    if(uart_buffer[0] < PROFILE_COUNT)
                    {
                        /* Lock the buffer before reading it */
                        profile transmit;
                        fetch_profile(transmit, uart_buffer[0]);
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
                if(flags & FLAG_PROFILE_UPDATED)
                {
                    save_profile(current_profile, globals.profile_order[globals.n_profile]);
                    flags &= ~FLAG_PROFILE_UPDATED;
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
                    memcpy(&(current_profile.devices[uart_buffer[1]]), (const void *) (uart_buffer + 2), DEVICE_LENGTH);
                    uart_flags &= ~UART_FLAG_LOCK;

                    convert_all_frames();

                    uart_transmit(RECEIVE_SUCCESS);

                    reset_uart();
                }
                break;
                //</editor-fold>
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
                break;
            }
#endif /* (COMPILE_EFFECTS !=0) */
            case DEMO_START_MUSIC:
            case DEMO_START_EFFECTS:
            {
#if (COMPILE_DEMOS != 0)
                demo = uart_control;
                frame = 0;
                reset_uart();
#else
                uart_transmit(DEMOS_DISABLED);
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
                        csgo_ctrl.bomb_tick_frame = 0;
                    }
                    if(new_state.bomb_state != csgo_state.bomb_state)
                    {
                        old_csgo_state.bomb_state = csgo_state.bomb_state;
                        csgo_state.bomb_state = new_state.bomb_state;
                        csgo_ctrl.bomb_overall_frame = 0;
                        csgo_ctrl.bomb_frame = 0;
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
                        csgo_ctrl.damage = csgo_ctrl.damage > DAMAGE_MIN_ON_DEATH ? csgo_ctrl.damage : DAMAGE_MIN_ON_DEATH;
                    }

                    if(csgo_state.bomb_state != old_csgo_state.bomb_state)
                    {
                        csgo_ctrl.bomb_frame = 0;
                        csgo_ctrl.bomb_overall_frame = 0;
                        csgo_ctrl.bomb_tick_rate = BOMB_TICK_INIT;
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
            }
        }
    }
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ImplicitIntegerAndEnumConversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#endif /* (COMPILE_UART != 0) */

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
#if (COMPILE_EFFECTS != 0)
    init_eeprom();
#endif /* (COMPILE_EFFECTS != 0) */

    while(1)
    {
        //<editor-fold desc="Buttons">
#if (COMPILE_BUTTONS != 0)
        if((PINA & PIN_BUTTON) && !(flags & FLAG_BUTTON))
        {
            button_frame = frame;
            flags |= FLAG_BUTTON;
        }
        else if(button_frame && !(PINA & PIN_BUTTON))
        {
            uint32_t time = frame - button_frame;
            if(time > BUTTON_MIN_FRAMES)
            {
                if(flags & FLAG_PROFILE_UPDATED)
                {
                    save_profile(current_profile, globals.profile_order[globals.n_profile]);
                    flags &= ~FLAG_PROFILE_UPDATED;
                }
                if(time < BUTTON_OFF_FRAMES && globals.leds_enabled)
                {
                    increment_profile();
                    save_globals();
                    refresh_profile();
                    uart_transmit(GLOBALS_UPDATED);
                    frame = 0;
                }
                else if(time < BUTTON_RESET_FRAMES)
                {
                    globals.leds_enabled = !globals.leds_enabled;
                    save_globals();
                    uart_transmit(GLOBALS_UPDATED);
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
            flags &= ~FLAG_NEW_FRAME;

#if (COMPILE_EFFECTS != 0)
            if(auto_increment && frame && frame % auto_increment == 0 && globals.leds_enabled)
            {
                if(flags & FLAG_PROFILE_UPDATED)
                {
                    save_profile(current_profile, globals.profile_order[globals.n_profile]);
                    flags &= ~FLAG_PROFILE_UPDATED;
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
                //csgo_state.ammo = 255 - (frame % UINT8_MAX);
                process_csgo(&csgo_ctrl, &csgo_state, &old_csgo_state, fan_buf, globals.fan_config[0], gpu_buf, pc_buf);

                /*if(!(frame % 64))
                    transmit_bytes((uint8_t *) &csgo_ctrl.bomb_overall_frame, 2);*/

                csgo_increment_frames();

                convert_bufs();
                apply_brightness();
            }
            else
            {
#endif /* (COMPILE_CSGO != 0) */
#if (COMPILE_EFFECTS != 0)
            if(globals.leds_enabled)
            {
                simple(pc_buf, DEVICE_PC);
                simple(gpu_buf, DEVICE_GPU);

                for(uint8_t i = 0; i < globals.fan_count; ++i)
                {
                    digital(fan_buf + FAN_LED_COUNT * i, FAN_LED_COUNT, globals.fan_config[i], DEVICE_FAN + i);
                }
                set_color(pc_buf, 0, 0, 0, 0);
                set_color(gpu_buf, 0, 0, 0, 0);

                /* Enable when the strip is installed */
                /*digital(strip_buf, STRIP_LED_COUNT, 0, DEVICE_STRIP);*/

                convert_bufs();
                apply_brightness();
            }
            else
            {
                set_all_colors(fan_buf, 0, 0, 0, FAN_LED_COUNT);
                set_all_colors(strip_buf, 0, 0, 0, STRIP_LED_COUNT + 1);

                set_color(pc_buf, 0, 0, 0, 0);
                set_color(gpu_buf, 0, 0, 0, 0);
            }
#endif /* (COMPILE_EFFECTS != 0) */
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
        }
    }
}

#pragma clang diagnostic pop

ISR(TIMER3_COMPA_vect)
{
    frame++;
    flags |= FLAG_NEW_FRAME;
    update();
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
