#include "uart.h"

void init_uart()
{
    UBRR0H = UBRRH_VALUE; /* Set high bits of baud rate */
    UBRR0L = UBRRL_VALUE; /* Set low bits of baud rate */

#if USE_2X
    UCSR0A |= (1 << U2X0);
#else
    UCSR0A &= ~((1 << U2X0));
#endif

    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); /* 8-bit data */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);   /* Enable RX and TX */
    UCSR0B |= (1 << RXCIE0); /* Enable RX interrupts */

    uart_transmit(UART_READY);
}

void uart_transmit(uint8_t data)
{
    while(!(UCSR0A & (1 << UDRE0))); /* Wait for the transmit register to be free */
    UDR0 = data;
}

uint8_t uart_receive()
{
    while((!(UCSR0A)) & (1 << RXC0)); /* Wait while data is being received */
    return UDR0;
}

void receive_bytes(uint8_t *p, uint16_t length)
{
    for(int i = 0; i < length; ++i)
    {
        p[i] = uart_receive();
    }
}

void transmit_bytes(uint8_t *p, uint16_t length)
{
    for(int i = 0; i < length; ++i)
    {
        uart_transmit(p[i]);
    }
}