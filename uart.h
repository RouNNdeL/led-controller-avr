#ifndef LEDCONTROLLER_UART_H
#define LEDCONTROLLER_UART_H

#include <avr/io.h>

#define SAVE_GLOBALS 0x10
#define SAVE_PROFILE 0x11
#define TEMP_DEVICE 0x12
#define SAVE_EXPLICIT 0x13
#define SAVE_PROFILE_FLAGS 0x14
#define FRAME_JUMP 0x15

#define SEND_GLOBALS 0x20
#define SEND_PROFILE 0x21

#define READY_TO_RECEIVE 0xA0
#define RECEIVE_SUCCESS 0xA1
#define UART_READY 0xA2

#define GLOBALS_UPDATED 0xB0
#define EFFECTS_DISABLED 0xBE

#define CSGO_BEGIN 0xC0
#define CSGO_NEW_STATE 0xC1
#define CSGO_DISABLED 0xCE
#define CSGO_END 0xCF

#define DEMO_START_MUSIC 0xD0
#define DEMO_START_EFFECTS 0xD1
#define DEMOS_DISABLED 0xDE
#define DEMO_END 0xDF

#define UNRECOGNIZED_COMMAND 0xE0
#define BUFFER_OVERFLOW 0xE1
#define PROFILE_OVERFLOW 0xE1

#define DEBUG_START 0xF0
#define DEBUG_STOP 0xF1
#define DEBUG_SET_FRAME 0xF2
#define DEBUG_GET_FRAME 0xF3
#define DEBUG_INCREMENT_FRAME 0xF4
#define DEBUG_NEW_INFO 0xF5
#define DEBUG_SEND_INFO 0xF6
#define DEBUG_DISABLED 0xFE
#define BAUD 9600

#include <util/setbaud.h>

void init_uart();

uint8_t uart_receive();

void uart_transmit(uint8_t data);

void receive_bytes(uint8_t *p, uint16_t length);

void transmit_bytes(uint8_t *p, uint16_t length);

#endif //LEDCONTROLLER_UART_H
