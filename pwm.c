#include <avr/io.h>

void pwm_init()
{
    DDRB |= (1 << PB1) | (1 << PB2) | (1 << PB3);
    DDRD |= (1 << PD3);

    TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM10);
    TCCR1B = _BV(CS12);

    TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM20);
    TCCR2B = _BV(CS22);

    OCR1A = 0;
    OCR1B = 0;
    OCR2A = 0;
    OCR2B = 0;
}