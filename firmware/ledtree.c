#include <avr/io.h>
#include <stdint.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#define F_CPU 6000000UL
#include <util/delay.h>

// Bit manipulation macros
#define sbi(a, b) ((a) |= 1 << (b))       //sets bit B in variable A
#define cbi(a, b) ((a) &= ~(1 << (b)))    //clears bit B in variable A
#define tbi(a, b) ((a) ^= 1 << (b))       //toggles bit B in variable A

void delay_ms(uint16_t d)
{
    uint16_t i;

    for(i = 0; i < d; i++)
        _delay_ms(1);
}

void pwm_setup(void)
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

    // Set PWM pins as outputs
    DDRB |= (1<<PB0)|(1<<PB1)|(1<<PB4);
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

int main(void)
{
    pwm_setup();
    flash_led();

//    set_led_color(255,255,255);
//    for(;;);

    while (1)
    {
        fade(255, 0, 0,     255, 255, 0,  100, 20);
        fade(255, 255, 0,   0, 255,   0,  100, 20);
        fade(  0, 255, 0,   0, 255, 255,  100, 20);
        fade(  0, 255, 255, 0,   0, 255,  100, 20);
        fade(  0,   0, 255, 255, 0, 255,  100, 20);
        fade(255,   0, 255, 255, 0,   0,  100, 20);
    }
    return 0;
}
