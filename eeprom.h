#include <avr/io.h>

#ifndef LEDCONTROLLER_EEPROM_H
#define LEDCONTROLLER_EEPROM_H

#ifndef __AVR_ATmega1284P__
#    error "This memory locations are only valid for an ATmega1284P"
#endif

#define CURRENT_PROFILE 0x00
#define PROFILE_COUNT 0x01
#define BRIGHTNESS 0x02
#define DEVICE_COUNT 0x03
#define STRIP_CONFIG 0x04
#define LEDS_ENABLED 0x05

#define PROFILES_OFFSET 0xFF
#define PROFILE_LENGTH 0x140
#define PROFILE(n) (n*PROFILE_LENGTH+PROFILES_OFFSET)

#define SINGLE_DEVICE_LENGTH 0x3A
#define SINGLE_DEVICE(profile, device) (PROFILE(profile)+SINGLE_DEVICE_LENGTH*device)

#define MODE_OFFSET 0x00
#define COLOR_COUNT_OFFSET 0x01
#define TIMING_OFFSET 0x02
#define ARGS_OFFSET 0x06
#define COLORS_OFFSET 0x0A
#define COLOR_LENGTH 0x03

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

#endif //LEDCONTROLLER_EEPROM_H