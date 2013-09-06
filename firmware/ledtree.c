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
#define RED    1  // PB1
#define GREEN  0  // PB0
#define BLUE   4  // PB4

#define EVENT_FALLING_EDGE 0
#define EVENT_RISING_EDGE  1
#define EVENT_NO_EVENT     2

// One clock tick = 8Mhz / 64 = 8us
#define BIT_DURATION  50 //us
#define TIMEOUT       12 * BIT_DURATION

void process_command(char *cmd);

volatile uint16_t g_event_time = 0;
volatile uint8_t g_event = EVENT_NO_EVENT;
volatile uint8_t g_data_lost = 0;

void setup(void)
{
    // PB4 = OC1B = 1k = blue
    // PB1 = OC0B = 560 = red
    // PB0 = 0C0A = 1k = green

    /* Set to Fast PWM */
//    TCCR0A |= _BV(WGM01) | _BV(WGM00) | _BV(COM0A1) | _BV(COM0B1);
//    GTCCR |= _BV(PWM1B) | _BV(COM1B1);

    // Reset timers and comparators
    OCR0A = 0;
    OCR0B = 0;
    OCR1B = 0;
    TCNT0 = 0;
    TCNT1 = 0;

    // Set the clock source
    TCCR0B |= _BV(CS00) | _BV(CS01);
    TCCR1 |= _BV(CS10) | _BV(CS11);

    // Turn on timer overflow interrupt
    TIMSK |= (1 << TOIE0);

    // PB2, PB3, PB5 are used for PARENT, CHILD0, CHILD1
    // but the port directions need to be decided when communication starts.

    // Setup PWM pins as output
    DDRB = (1 << PB0) | (1 << PB1) | (1 << PB4);

    // PCINT setup
    PCMSK |= (1 << PCINT3);
    GIMSK |= (1 << PCIE);
}

volatile uint16_t g_timer = 0;

ISR(PCINT0_vect)
{
    if (g_event != EVENT_NO_EVENT)
    {
        g_data_lost = 1;
        g_event_time = g_timer + TCNT0;
        return;
    }
    g_event_time = g_timer + TCNT0;
    g_event = PINB & (1 << CHILD0) ? EVENT_RISING_EDGE : EVENT_FALLING_EDGE;
}

ISR(TIMER0_OVF_vect)
{
    g_timer += 256;
}

uint8_t get_event(uint16_t *event_time)
{
    uint8_t event;

    cli();
    event = g_event;
    g_event = EVENT_NO_EVENT;
    *event_time = g_event_time;
    sei();

    return event;
}

uint16_t elapsed_time(uint16_t start, uint16_t end)
{
    // if our counter has not rolled over, its simple math
    if (start < end)
        return end - start;

    // it rolled, so we need to take that into account
    return 65535 - start + end; 
}

void delay_ms(uint16_t d)
{
    uint16_t i;

    for(i = 0; i < d; i++)
        _delay_ms(1);
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

uint8_t send_byte(uint8_t port, uint8_t ch)
{
    uint8_t i;

    // send the start/duration bit
    sbi(PORTB, port);
    _delay_us(BIT_DURATION);

    cbi(PORTB, port);
    _delay_us(BIT_DURATION);

    for(i = 0; i < 8; i++)
    {
        sbi(PORTB, port);
        _delay_us(BIT_DURATION);

        if (ch & (1 << i))
        {
            _delay_us(BIT_DURATION);
            cbi(PORTB, port);
        }
        else
        {
            cbi(PORTB, port);
            _delay_us(BIT_DURATION);
        }
        _delay_us(BIT_DURATION);
    }

    // Remove these later. they are here for debugging
    _delay_us(BIT_DURATION);
    _delay_us(BIT_DURATION);

    return 0;
}

void send_command(uint8_t port, char *cmd)
{
    char *ptr;

    for(ptr = cmd; *ptr; ptr++)
        send_byte(port, *ptr);
}

#define MAX_CMD_LEN 32
static char g_cmd[MAX_CMD_LEN];
static uint8_t cmd_len = 0;

uint8_t process_io(char *cmd)
{
    static uint8_t event, data;
    static uint8_t bit_duration = 0;
    static uint8_t bit_count = 0;
    static uint16_t duration, event_time, start_t;

    event = get_event(&event_time);

    // Check to see if we have an event to process
    if (event == EVENT_NO_EVENT)
        return 0;

    // If we've lost data, flash the red led and quit!
    if (g_data_lost)
        for(;;)
        {
            tbi(PORTB, RED);
            tbi(PORTB, GREEN);
            tbi(PORTB, BLUE);
            _delay_ms(100);
        }

    // If we have a rising edge, note its time and bail
    if (event == EVENT_RISING_EDGE)
    {
        start_t = event_time;
        return 0;
    }

    // From here on out, its all processing falling edge events

    duration = elapsed_time(start_t, event_time);
//    if (duration < 25)
//        tbi(PORTB, BLUE);

    // FIXME
    if (0 && duration > TIMEOUT)
    {
        bit_count = 0;
        bit_duration = 0;
        cmd_len = 0;
        g_cmd[0] = 0;
    }

    // For falling edge, check measure bit_duration if we don't have it
    if (bit_duration == 0)
    {
        bit_duration = (uint8_t)duration;
        bit_count = 0;
        data = 0;
        return 0;
    }

    // We have a falling edge and know bit_duration, then we have data!
    if (abs((uint8_t)duration - bit_duration) > (bit_duration >> 1))
    {
        data |= (1 << bit_count);
    }

    bit_count++;
    if (bit_count == 8)
    {
        bit_count = 0;
        tbi(PORTB, GREEN);
        bit_duration = 0;

        if (data == 's')
            tbi(PORTB, RED);

        if (data == '\n')
        {
            strcpy(cmd, g_cmd);
            return 1;
        }

        g_cmd[cmd_len++] = data;
        g_cmd[cmd_len] = 0;
    }
    return 0;
}

void process_command(char *cmd)
{
    if (strcmp(cmd, "boo!") == 0)
        set_led_color(0, 0, 255);
    else
        set_led_color(255, 0, 0);
}

void __main(void)
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

#define BB_MASTER 0
#if BB_MASTER
int main(void)
{
    setup();
    flash_led();

    DDRB |= (1 << CHILD0);
    for(;;)
    {
        send_command(CHILD0, "sex\n");
        _delay_ms(1000);
        send_command(CHILD0, "drugs\n");
        _delay_ms(1000);
    }

    return 0;
}
#else
int main(void)
{
    uint8_t i;
    char cmd[MAX_CMD_LEN];

    setup();
    _delay_ms(2000);
    flash_led();
    g_cmd[0] = 0;
    sei();

    for(;;)
    {
        if (process_io(cmd))
        {
            if (strcmp(cmd, "sex") == 0)
            {
                sbi(PORTB, BLUE);
//                set_led_color(0, 0, 255);
                _delay_ms(500);
            }
            else
            {
                cbi(PORTB, BLUE);
//                set_led_color(255, 0, 0);
                _delay_ms(500);
            }
        }
    }

    return 0;
}
#endif
