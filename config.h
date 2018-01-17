#ifndef LEDCONTROLLER_CONFIG_H
#define LEDCONTROLLER_CONFIG_H

#define FPS 64
#define COMPILE_DEMOS 0 /* Demos take up a lot of memory, disable them here */
#define COMPILE_CSGO 0
#define COMPILE_UART 1
#define COMPILE_BUTTONS 0
#define COMPILE_EFFECTS 0

/* Note: These change the color representation of a color */
#define ACTUAL_BRIGHTNESS_DIGITAL 1 /* Whether to compile in use of log brightness function for WS2812B */
#define ACTUAL_BRIGHTNESS_ANALOG1 1 /* Whether to compile in use of log brightness function for PC */
#define ACTUAL_BRIGHTNESS_ANALOG2 1 /* Whether to compile in use of log brightness function for GPU */

#define BUTTON_MIN_FRAMES 3
#define BUTTON_OFF_FRAMES 1 * FPS
#define BUTTON_RESET_FRAMES 5 * FPS

#define PROFILE_COUNT 8

#define FAN_LED_COUNT 12
#define MAX_FAN_COUNT 3
#define STRIP_LED_COUNT 0

#define AUTO_INCREMENT_MULTIPLIER 4

#endif //LEDCONTROLLER_CONFIG_H
