#ifndef LEDCONTROLLER_CONFIG_H
#define LEDCONTROLLER_CONFIG_H

#define FPS 64
#define COMPILE_DEMOS 0 /* Demos take up a lot of memory, disable them here */
#define COMPILE_CSGO 1
#define COMPILE_UART 1
#define COMPILE_BUTTONS 1
#define COMPILE_EFFECTS 1
#define COMPILE_DEBUG 1 /* Allow pausing the effect and seeking frame by frame */

#define DEBUG_BUFFER_SIZE 26

#if (COMPILE_EFFECTS == 0 && COMPILE_DEMOS == 0 && COMPILE_CSGO == 0)
#    warning All intercations have been disabled, this state is undesirable in production
#endif /* (COMPILE_EFFECTS == 0 && COMPILE_DEMOS == 0 && COMPILE_CSGO == 0) */

#if (COMPILE_DEBUG != 0 && COMPILE_UART == 0)
#    error Debugging has been enabled but UART has not. Debugging won't work without UART
#endif /* (COMPILE_DEBUG != 0 && COMPILE_UART == 0) */

/* Note: These change the color representation of a color */
#define ACTUAL_BRIGHTNESS_DIGITAL 1 /* Whether to compile in use of log brightness function for WS2812B */
#define ACTUAL_BRIGHTNESS_ANALOG1 1 /* Whether to compile in use of log brightness function for PC */
#define ACTUAL_BRIGHTNESS_ANALOG2 1 /* Whether to compile in use of log brightness function for GPU */

#define BUTTON_MIN_FRAMES 3
#define BUTTON_OFF_FRAMES 1 * FPS
#define BUTTON_RESET_FRAMES 5 * FPS

#define PROFILE_COUNT 8
#define COLOR_COUNT 16
#define DEVICE_COUNT 6
#define ARG_COUNT 5
#define TIME_COUNT 6

#define FAN_LED_COUNT 12
#define MAX_FAN_COUNT 3
#define STRIP_SIDE_LED_COUNT 24
#define STRIP_FRONT_LED_COUNT 7
#define STRIP_FRONT_VIRTUAL_COUNT 12
#define STRIP_LED_COUNT (2 * STRIP_SIDE_LED_COUNT + STRIP_FRONT_LED_COUNT)
#define STRIP_VIRTUAL_COUNT (2 * STRIP_SIDE_LED_COUNT + STRIP_FRONT_VIRTUAL_COUNT)

#endif //LEDCONTROLLER_CONFIG_H
