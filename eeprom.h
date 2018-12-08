#include <avr/io.h>
#include "config.h"

#ifndef LEDCONTROLLER_EEPROM_H
#define LEDCONTROLLER_EEPROM_H

typedef struct
{
    uint8_t effect;
    uint8_t color_count;
    uint8_t timing[TIME_COUNT];
    uint8_t args[ARG_COUNT];
    uint8_t colors[COLOR_COUNT * 3];
} __attribute__((packed)) device_profile;

typedef struct
{
    device_profile devices[DEVICE_COUNT];
} __attribute__((packed)) profile;

typedef struct
{
    uint8_t brightness[DEVICE_COUNT];
    uint8_t color[DEVICE_COUNT][3];
    uint8_t flags[DEVICE_COUNT];
    uint8_t current_device_profile[DEVICE_COUNT];
    uint8_t profile_count;
    uint8_t current_profile;
    uint8_t fan_count;
    uint8_t auto_increment;
    uint8_t fan_config[MAX_FAN_COUNT];
    int8_t profiles[PROFILE_COUNT][DEVICE_COUNT];
    uint8_t profile_flags[PROFILE_COUNT];
} __attribute__((packed)) global_settings;

#define GLOBALS_LENGTH sizeof(global_settings)
#define DEVICE_LENGTH sizeof(device_profile)
#define PROFILE_LENGTH sizeof(profile)

#define DEVICE_FLAG_ENABLED (1 << 0)
#define DEVICE_FLAG_EFFECT_ENABLED (1 << 1)
#define DEVICE_FLAG_PROFILE_UPDATED (1 << 2)
#define DEVICE_FLAG_TRANSITION (1 << 3)

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

#define DEVICE_INDEX_PC 0
#define DEVICE_INDEX_GPU 1
#define DEVICE_INDEX_FAN(n) 2+n
#define DEVICE_INDEX_STRIP DEVICE_INDEX_FAN(MAX_FAN_COUNT)

#endif //LEDCONTROLLER_EEPROM_H