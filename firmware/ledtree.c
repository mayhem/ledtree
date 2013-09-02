#include <avr/io.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// Bit manipulation macros
#define sbi(a, b) ((a) |= 1 << (b))       //sets bit B in variable A
#define cbi(a, b) ((a) &= ~(1 << (b)))    //clears bit B in variable A
#define tbi(a, b) ((a) ^= 1 << (b))       //toggles bit B in variable A

#define PARENT 2  // PB2
#define CHILD0 3  // PB3
#define CHILD1 5  // PB5

void process_command(char *cmd);

void delay_ms(uint16_t d)
{
    uint16_t i;

    for(i = 0; i < d; i++)
        _delay_ms(1);
}

void setup(void)
{
    // PB4 = OC1B = 1k = blue
    // PB1 = OC0B = 560 = red
    // PB0 = 0C0A = 1k = green

    /* Set to Fast PWM */
    TCCR0A |= _BV(WGM01) | _BV(WGM00) | _BV(COM0A1) | _BV(COM0B1);
    GTCCR |= _BV(PWM1B) | _BV(COM1B1);

    // Reset timers and comparators
    OCR0A = 0;
    OCR0B = 0;
    OCR1B = 0;
    TCNT0 = 0;
    TCNT1 = 0;

    // Set the clock source
    TCCR0B |= _BV(CS00);
    TCCR1 |= _BV(CS10);

    // PB2, PB3, PB5 are used for PARENT, CHILD0, CHILD1
    // but the port directions need to be decided when communication starts.
    DDRB = (1 << PB0) | (1 << PB1) | (1 << PB4);
}

void set_led_color(uint8_t red, uint8_t green, uint8_t blue)
{
    OCR1B = blue;
    OCR0B = red;
    OCR0A = green;
}

void flash_led(void)
{
    uint8_t i;

    for(i = 0; i < 5; i++)
    {
        set_led_color(255, 255, 255);
        _delay_ms(100);
        set_led_color(0, 0, 0);
        _delay_ms(100);
    }
    set_led_color(0, 0, 0);
}

int fade_test(void)
{
    uint8_t i;

    while (1)
    {
        for(i = 0; i < 255; i++)
        {
            set_led_color(i, 0, 0);
            _delay_ms(20); 
        }
        set_led_color(0, 0, 0);
        for(i = 0; i < 255; i++)
        {
            set_led_color(0, i, 0);
            _delay_ms(20); 
        }
        set_led_color(0, 0, 0);
        for(i = 0; i < 255; i++)
        {
            set_led_color(0, 0, i);
            _delay_ms(20); 
        }
        set_led_color(0, 0, 0);
    }
    return 0;
}

void fade(uint8_t r1, uint8_t g1, uint8_t b1,
          uint8_t r2, uint8_t g2, uint8_t b2,
          uint8_t steps, uint8_t delay)
{
    int16_t i;

    for(i = 0; i < steps; i++)
    {
        set_led_color((uint8_t)(r1 + (uint8_t)(((int16_t)(r2 - r1) * i) / steps)),
                      (uint8_t)(g1 + (uint8_t)(((int16_t)(g2 - g1) * i) / steps)),
                      (uint8_t)(b1 + (uint8_t)(((int16_t)(b2 - b1) * i) / steps)));
        delay_ms(delay); 
    }
}

#define BIT_DELAY 102
#define HALF_BIT_DELAY 52
uint8_t send_byte(uint8_t port, uint8_t ch)
{
    uint8_t i;

    // send the start bit
    cbi(PORTB, port);
    _delay_us(BIT_DELAY);

    for(i = 0; i < 8; i++)
    {
        if (ch & (1 << i))
            sbi(PORTB, port);
        else
            cbi(PORTB, port);
        _delay_us(BIT_DELAY);
    }

    // Send the stop bit
    sbi(PORTB, port);
    _delay_us(BIT_DELAY);

    return 0;
}

uint8_t receive_byte(uint8_t port)
{
    uint8_t i, ch = 0;

    // Wait for the line to become quiet
    for(;;)
        if (PINB & (1 << port))
            break;

    // wait for the start bit
    for(;;)
        if (!(PINB & (1 << port)))
            break;

    _delay_us(HALF_BIT_DELAY);
    for(i = 0; i < 8; i++)
    {
        _delay_us(BIT_DELAY);

        if (PINB & (1 << port))
            ch |= (1 << i);
    }    

    // Ride out the stop bit
    _delay_us(BIT_DELAY);

    return ch;
}
uint8_t send_command(uint8_t port, char *cmd)
{
    uint8_t num = 0, ack;
    char *ptr;

    for(;;)
    {
        // Prepare to send over the given pin
        DDRB |= (1<<port);

        for(ptr = cmd; *ptr; ptr++)
            send_byte(port, *ptr);

        // Prepare to receive over the given pin
        DDRB &= ~(1<<port);

        ack = receive_byte(port);
        if (ack == 0)
            break;
    }

    ptr = cmd;
    for(;;)
    {
        *ptr = receive_byte(port);
        if (*ptr == '\n')
        {
            *ptr = 0;
            return num;
        }
        ptr++;
        num++;
    }

    // Switch back to transmit
    DDRB |= (1<<port);

    // Send ack
    send_byte(port, 0);
}

uint8_t receive_command(uint8_t port, char *cmd)
{
    uint8_t num = 0, ack;
    char *ptr = cmd;

    // Prepare to receive over the given pin
    DDRB &= ~(1<<port);

    for(;;)
    {
        *ptr = receive_byte(port);
        if (*ptr == '\n')
        {
            *ptr = 0;
            return num;
        }
        ptr++;
        num++;
    }

    // Switch to transmit
    DDRB |= (1<<port);
    send_byte(port, 0);

    process_command(cmd);

    for(;;)
    {
        // Prepare to send over the given pin
        DDRB |= (1<<port);

        for(ptr = cmd; *ptr; ptr++)
            send_byte(port, *ptr);

        // Prepare to receive over the given pin
        DDRB &= ~(1<<port);

        ack = receive_byte(port);
        if (ack == 0)
            break;
    }

    // Back to receive
    DDRB &= ~(1<<port);
}

void process_command(char *cmd)
{
    if (strcmp(cmd, "boo!") == 0)
        set_led_color(0, 0, 255);
    else
        set_led_color(255, 0, 0);
}

void rainbow_main(void)
{
    setup();
    flash_led();

    while (1)
    {
        fade(255, 0, 0,     255, 255, 0,  100, 20);
        fade(255, 255, 0,   0, 255,   0,  100, 20);
        fade(  0, 255, 0,   0, 255, 255,  100, 20);
        fade(  0, 255, 255, 0,   0, 255,  100, 20);
        fade(  0,   0, 255, 255, 0, 255,  100, 20);
        fade(255,   0, 255, 255, 0,   0,  100, 20);
    }
}

#define BB_MASTER 1
#if BB_MASTER
int main(void)
{
    uint8_t i, ch;

    setup();
    flash_led();

    DDRB |= (1 << CHILD0);
    for(;;)
    {
        send_byte(CHILD0, 'A');
        _delay_ms(1000);
        send_byte(CHILD0, 'Z');
        _delay_ms(1000);
    }

    return 0;
}
#else
int main(void)
{
    uint8_t i, ch;

    setup();
    flash_led();
    for(;;)
    {
        ch = receive_byte(PARENT);
        if (ch == 'A')
        {
            set_led_color(0, 0, 255);
            _delay_ms(500);
        }
        else
        {
            set_led_color(255, 0, 0);
            _delay_ms(500);
        }
    }

    return 0;
}
#endif
