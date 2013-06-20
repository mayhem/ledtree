#include <avr/io.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#define F_CPU 16000000UL
#include <util/delay.h>

// Bit manipulation macros
#define sbi(a, b) ((a) |= 1 << (b))       //sets bit B in variable A
#define cbi(a, b) ((a) &= ~(1 << (b)))    //clears bit B in variable A
#define tbi(a, b) ((a) ^= 1 << (b))       //toggles bit B in variable A

#define PARENT PD7
#define CHILD0 PD4
#define CHILD1 PD2

void process_command(char *cmd);

void delay_ms(uint16_t d)
{
    uint16_t i;

    for(i = 0; i < d; i++)
        _delay_ms(1);
}

void pwm_setup(void)
{
    /* Set to Fast PWM */
    TCCR0A |= _BV(WGM01) | _BV(WGM00);
    TCCR2A |= _BV(WGM21) | _BV(WGM20);

    // Set the compare output mode
    TCCR0A |= _BV(COM0A1);
    TCCR0A |= _BV(COM0B1);
    TCCR2A |= _BV(COM2B1);

    // Reset timers and comparators
    OCR0A = 0;
    OCR0B = 0;
    OCR2B = 0;
    TCNT0 = 0;
    TCNT2 = 0;

    // Set the clock source
    TCCR0B |= _BV(CS00);
    TCCR2B |= _BV(CS20);

    // Set PWM pins as outputs
    DDRD |= (1<<PD6)|(1<<PD5)|(1<<PD3);
}

void set_led_color(uint8_t red, uint8_t green, uint8_t blue)
{
    OCR2B = green;
    OCR0B = red;
    OCR0A = blue;
}

void flash_led(void)
{
    uint8_t i;

    for(i = 0; i < 3; i++)
    {
        set_led_color(0, 0, 255);
        _delay_ms(200);
        set_led_color(255, 0, 0);
        _delay_ms(200);
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

#define BIT_DELAY 100

uint8_t receive_byte(uint8_t port)
{
    uint8_t i, ch = 0;

    // Wait for the line to become quiet
    for(;;)
        if (!(PIND & (1 << port)))
            break;

    // wait for the start bit
    for(;;)
        if (PIND & (1 << port))
            break;

    for(i = 0; i < 8; i++)
    {
        _delay_us(BIT_DELAY);

        if (PIND & (1 << port))
            ch |= (1 << i);
    }    

    // Ride out the stop bit
    _delay_us(BIT_DELAY);

    return ch;
}

uint8_t send_byte(uint8_t port, uint8_t ch)
{
    uint8_t i;

    // send the start bit
    sbi(PORTD, port);
    _delay_us(BIT_DELAY);

    for(i = 0; i < 8; i++)
    {
        if (ch & (1 << i))
            sbi(PORTD, port);
        else
            cbi(PORTD, port);
        _delay_us(BIT_DELAY);
    }

    // Send the stop bit
    cbi(PORTD, port);
    _delay_us(BIT_DELAY);
}

uint8_t send_command(uint8_t port, char *cmd)
{
    uint8_t num = 0, ack;
    char *ptr;

    for(;;)
    {
        // Prepare to send over the given pin
        DDRD |= (1<<port);

        for(ptr = cmd; *ptr; ptr++)
            send_byte(port, *ptr);

        // Prepare to receive over the given pin
        DDRD &= ~(1<<port);

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
    DDRD |= (1<<port);

    // Send ack
    send_byte(port, 0);
}

uint8_t receive_command(uint8_t port, char *cmd)
{
    uint8_t num = 0, ack;
    char *ptr = cmd;

    // Prepare to receive over the given pin
    DDRD &= ~(1<<port);

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
    DDRD |= (1<<port);
    send_byte(port, 0);

    process_command(cmd);

    for(;;)
    {
        // Prepare to send over the given pin
        DDRD |= (1<<port);

        for(ptr = cmd; *ptr; ptr++)
            send_byte(port, *ptr);

        // Prepare to receive over the given pin
        DDRD &= ~(1<<port);

        ack = receive_byte(port);
        if (ack == 0)
            break;
    }

    // Back to receive
    DDRD &= ~(1<<port);
}

void process_command(char *cmd)
{
    if (strcmp(cmd, "boo!") == 0)
        set_led_color(0, 0, 255);
    else
        set_led_color(255, 0, 0);
}

int main(void)
{
    char cmd[32];

    pwm_setup();
    flash_led();

#if 1
    strcpy(cmd, "boo!");
    send_command(CHILD0, cmd);

    if (strcmp(cmd, "boo!") == 0)
        set_led_color(0, 0, 255);
    else
        set_led_color(255, 0, 0);
#else
    for(;;)
        receive_command(PARENT, cmd);
#endif

    return 0;
}
