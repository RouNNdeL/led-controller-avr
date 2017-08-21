#ifndef LEDCONTROLLER_EEPROM_H
#    define LEDCONTROLLER_EEPROM_H

#endif //LEDCONTROLLER_EEPROM_H
#ifndef __AVR_ATmega1284P__
#    error "This memory locations are only valid for an ATmega1284P"
#else

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