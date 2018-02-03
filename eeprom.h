#include <avr/io.h>
#include "config.h"

#ifndef LEDCONTROLLER_EEPROM_H
#define LEDCONTROLLER_EEPROM_H

#ifndef __AVR_ATmega1284P__
//#    error "This memory locations are only valid for an ATmega1284P"
#endif

typedef struct
{
    uint8_t effect;
    uint8_t color_count;
    uint8_t color_cycles;
    uint8_t timing[6];
    uint8_t args[5];
    uint8_t colors[16 * 3];
} __attribute__((packed)) device_profile;

typedef struct
{
    device_profile devices[6];
    uint8_t flags;
} __attribute__((packed)) profile;

typedef struct
{
    uint8_t brightness;
    uint8_t profile_count;
    uint8_t n_profile;
    uint8_t leds_enabled;
    uint8_t fan_count;
    uint8_t auto_increment;
    uint8_t fan_config[3];
    uint8_t profile_order[PROFILE_COUNT];
} __attribute__((packed)) global_settings;

#define GLOBALS_LENGTH sizeof(global_settings)
#define DEVICE_LENGTH sizeof(device_profile)
#define PROFILE_LENGTH sizeof(profile)

/**
 * When set to 1 the strip functions like a loop, otherwise both sides function like one strip
 */
#define PROFILE_FLAG_STRIP_MODE (1 << 0)

/**
 * When set to 1 the front functions as the last led of the strip, otherwise it wraps to front
 * (only applies when <code>PROFILE_FLAG_STRIP_MODE</code> is set to single strip mode)
 */
#define PROFILE_FLAG_FRONT_MODE (1 << 1)

/** When set to 1 the front is set to function like the pc_buf, otherwise it is the part of the bottom strip
 * and it behaviour depends on PROFILE_FLAG_FRONT_MODE
 */
#define PROFILE_FLAG_FRONT_PC (1 << 2)

#define DEVICE_PC 0
#define DEVICE_GPU 1
#define DEVICE_FAN 2
#define DEVICE_STRIP DEVICE_FAN + MAX_FAN_COUNT

#endif //LEDCONTROLLER_EEPROM_H