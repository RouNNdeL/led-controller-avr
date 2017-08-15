#include <avr/io.h>
#include <avr/delay.h>

#define INPUT 0
#define OUTPUT 1

int main(void)
{
    DDRC |= (OUTPUT << PC1);

    while (1)
    {
        _delay_ms(1000);
        PINC ^= (1 << PC1);
    }
}