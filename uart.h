#ifndef LEDCONTROLLER_UART_H
#define LEDCONTROLLER_UART_H

#include <avr/io.h>

#define SAVE_GLOBALS 0x10
#define SAVE_PROFILE 0x11

#define SEND_GLOBALS 0x20
#define SEND_PROFILE 0x21

#define READY_TO_RECEIVE 0xA0
#define RECEIVE_SUCCESS 0xA1
#define UART_READY 0xA2

#define GLOBALS_UPDATED 0xB0

#define UNRECOGNIZED_COMMAND 0xE0
#define BUFFER_OVERFLOW 0xE1

#define BAUD 9600

#include <util/setbaud.h>

void init_uart();

uint8_t uart_receive();

void uart_transmit(uint8_t data);

void receive_bytes(uint8_t *p, uint16_t length);

void transmit_bytes(uint8_t *p, uint16_t length);

#endif //LEDCONTROLLER_UART_H
