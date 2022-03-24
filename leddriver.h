/*
    80x7 red-green LED display driver.

    This driver controls a LED display, organized as 7 rows of 80 red-green pixels each.

    The rows are controlled by a 3-to-8 multiplexer.
    Rows 0-6 are actual rows on the display. Row 7 is used as the 'disabled' row.

    The columns consist of two 80-bit shift registers (one for red, one for green).
    The red and green shift registers have their own data-bit, but share a common shift signal and latch signal.
    A low data value lights up the LED. Intermediate values are shown through temporal dithering (at 500 Hz).

    This driver essentially runs as an interrupt routine for each horizontal line ("hsync").
    The routine activates the *previous* line by setting the multiplexer and latching the shift register data.
    Then it prepares the data for the current line by writing the contents of the shift registers.
    During the "8th" line, no data is displayed, the driver notifies this through the "vsync" callback.

    API:
    -   led_init(vsync_callback)    Initializes the display, blanks the LEDs, sets the vsync callback.
                                    The vsync callback should be used to write a new frame.
    -   led_enable()                Starts the driver and enables output to the hardware.
    -   led_write_framebuffer(data) Provides new framebuffer data to the driver.
    -   led_disable()               Stops the driver and disables output to the hardware.
 */


#include <stdint.h>

typedef void (vsync_fn_t) (int frame_nr);

void led_init(vsync_fn_t * vsync);
void led_write_framebuffer(const void *data);

void led_enable(void);
void led_disable(void);

