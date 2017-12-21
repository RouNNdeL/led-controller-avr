#include <avr/io.h>

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
} __attribute__((packed)) profile;

typedef struct
{
    uint8_t brightness;
    uint8_t profile_count;
    uint8_t n_profile;
    uint8_t leds_enabled;
    uint8_t fan_config[3];
} __attribute__((packed)) global_settings;

#define GLOBALS_LENGTH sizeof(global_settings)
#define DEVICE_LENGTH sizeof(device_profile)
#define PROFILE_LENGTH sizeof(profile)

#endif //LEDCONTROLLER_EEPROM_H