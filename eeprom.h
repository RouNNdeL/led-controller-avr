#include <avr/io.h>

#ifndef LEDCONTROLLER_EEPROM_H
#define LEDCONTROLLER_EEPROM_H

#ifndef __AVR_ATmega1284P__
//#    error "This memory locations are only valid for an ATmega1284P"
#endif

#define PROFILE_LENGTH 0x140

typedef struct
{
    uint8_t mode;
    uint8_t color_count;
    uint8_t timing[4];
    uint8_t args[4];
    uint8_t colors[16 * 3];
} device_profile;

typedef struct
{
    uint8_t name[30];
    device_profile devices[5];
} profile;

typedef struct
{
    uint8_t brightness;
    uint8_t profile_count;
    uint8_t n_profile;
    uint8_t leds_enabled;
} global_settings;

#endif //LEDCONTROLLER_EEPROM_H