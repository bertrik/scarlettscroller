#include <string.h>

#include <Arduino.h>

#include "framebuffer.h"
#include "leddriver.h"

#define PIN_A       D2
#define PIN_B       D3
#define PIN_C       D4
#define PIN_SCLK    D5
#define PIN_R       D6
#define PIN_G       D7

static const vsync_fn_t *vsync_fn;
static uint8_t framebuffer[LED_HEIGHT][LED_WIDTH];
static uint8_t pwmbuf[LED_HEIGHT][LED_WIDTH];
static int row = 0;
static int frame = 0;

#define FAST_GPIO_WRITE(pin,val) if (val) GPOS = 1<<(pin); else GPOC = 1<<(pin)

static void rowselect(int select)
{
    // set the row multiplexer
    FAST_GPIO_WRITE(PIN_A, select & 1);
    FAST_GPIO_WRITE(PIN_B, select & 2);
    FAST_GPIO_WRITE(PIN_C, select & 4);
}

// "horizontal" interrupt routine, displays one line
static void IRAM_ATTR led_hsync(void)
{
    // blank the display by selecting row 0
    rowselect(0);

    // write column data
    if (row < 7) {
        // write the column shift registers
        uint8_t *rowbuf = pwmbuf[row];
        for (int col = 0; col < LED_WIDTH; col++) {
            int val = rowbuf[col];
            val += framebuffer[row][col];
            rowbuf[col] = val;
            bool c = val > 255;

            // write data
            FAST_GPIO_WRITE(PIN_SCLK, 0);
            FAST_GPIO_WRITE(PIN_G, c);
            FAST_GPIO_WRITE(PIN_SCLK, 1);
        }

        // next row and set row multiplexer
        row++;
        rowselect(row);
    } else {
        // a dark vsync frame, allows application to update the frame buffer
        vsync_fn(frame++);
        row = 0;
    }
}

void led_write_framebuffer(const void *data)
{
    memcpy(framebuffer, data, sizeof(framebuffer));
}

void led_init(const vsync_fn_t * vsync)
{
    // set all pins to a defined state
    pinMode(PIN_A, OUTPUT);
    digitalWrite(PIN_A, 0);

    pinMode(PIN_B, OUTPUT);
    digitalWrite(PIN_B, 0);

    pinMode(PIN_C, OUTPUT);
    digitalWrite(PIN_C, 0);

    pinMode(PIN_R, OUTPUT);
    digitalWrite(PIN_R, 0);

    pinMode(PIN_G, OUTPUT);
    digitalWrite(PIN_G, 0);

    pinMode(PIN_SCLK, OUTPUT);
    digitalWrite(PIN_SCLK, 0);

    // copy vsync pointer
    vsync_fn = vsync;

    // clear the frame buffer
    memset(framebuffer, 0, sizeof(framebuffer));

    // initialise PWM buffer
    for (int y = 0; y < LED_HEIGHT; y++) {
        for (int x = 0; x < LED_WIDTH; x++) {
            pwmbuf[y][x] = random(255);
        }
    }

    // initialise timer
    timer1_isr_init();
}

void led_enable(void)
{
    // set up timer interrupt
    timer1_disable();
    timer1_attachInterrupt(led_hsync);
    timer1_write(500);        // fps = 555555/number
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
}

void led_disable(void)
{
    // detach the interrupt routine
    timer1_detachInterrupt();
    timer1_disable();
    
    // select invisible row
    rowselect(0);
}

