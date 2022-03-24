#include <string.h>

#include <Arduino.h>

#include "framebuffer.h"
#include "leddriver.h"

#define PIN_D       D1
#define PIN_A       D2
#define PIN_B       D3
#define PIN_C       D4
#define PIN_SCLK    D5
#define PIN_R       D6      
#define PIN_G       D7

static const vsync_fn_t *vsync_fn;
static uint8_t framebuffer[LED_HEIGHT][LED_WIDTH];
static int row = 0;
static int frame = 0;

#define FAST_GPIO_WRITE(pin,val) if (val) GPOS = 1<<(pin); else GPOC = 1<<(pin)

// "horizontal" interrupt routine, displays one line
static void IRAM_ATTR led_hsync(void)
{
    // deactivate rows while updating column data and row multiplexer
    FAST_GPIO_WRITE(PIN_D, 0);

    // write column data
    if (row < 8) {
        // write the column shift registers
        uint8_t *fb_row = framebuffer[row];
        for (int col = 0; col < LED_WIDTH; col++) {
            uint8_t c = fb_row[col] ? 1 : 0;

            // write data
            FAST_GPIO_WRITE(PIN_SCLK, 0);
            FAST_GPIO_WRITE(PIN_R, c);
            FAST_GPIO_WRITE(PIN_SCLK, 1);
        }
        // set the row multiplexer
        FAST_GPIO_WRITE(PIN_A, row & 1);
        FAST_GPIO_WRITE(PIN_B, row & 2);
        FAST_GPIO_WRITE(PIN_C, row & 4);

        // activate row
        FAST_GPIO_WRITE(PIN_D, 1);
        
        // next row
        row++;
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
    pinMode(PIN_D, OUTPUT);
    digitalWrite(PIN_D, 0);
    pinMode(PIN_R, OUTPUT);
    digitalWrite(PIN_R, 0);
    pinMode(PIN_SCLK, OUTPUT);
    digitalWrite(PIN_SCLK, 0);

    // copy vsync pointer
    vsync_fn = vsync;

    // clear the frame buffer
    memset(framebuffer, 0, sizeof(framebuffer));

    // initialise timer
    timer1_isr_init();
}

void led_enable(void)
{
    // set up timer interrupt
    timer1_disable();
    timer1_attachInterrupt(led_hsync);
    timer1_write(1250); // fps = 555555/number
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
}

void led_disable(void)
{
    // detach the interrupt routine
    timer1_detachInterrupt();
    timer1_disable();

    // disable row output
    digitalWrite(PIN_D, 0);
}


