#include <string.h>
#include <math.h>

#include "font.h"
#include "framebuffer.h"
#include "draw.h"

static uint8_t *_framebuffer;
static uint8_t gamma_corr[256];
static bool fliph, flipv;

void draw_init(uint8_t *framebuffer, double gamma)
{
    _framebuffer = framebuffer;
    for (int i = 0; i < 256; i++) {
        gamma_corr[i] = round(pow(i / 255.0, gamma) * 255);
    }
    fliph = false;
    flipv = false;
}

void draw_flip(bool horizontal, bool vertical)
{
    fliph = horizontal;
    flipv = vertical;
}

void draw_clear(void)
{
    memset(_framebuffer, 0, LED_WIDTH * LED_HEIGHT);
}

bool draw_pixel(int x, int y, uint8_t c)
{
    if ((x < 0) || (x >= LED_WIDTH) || (y < 0) || (y >= LED_HEIGHT)) {
        return false;
    }
    if (fliph) {
        x = LED_WIDTH - 1 - x;
    }
    if (flipv) {
        y = LED_HEIGHT - 1 - y;
    }
    _framebuffer[y * LED_WIDTH + x] = gamma_corr[c];
    return true;
}

void draw_vline(int x, uint8_t c)
{
    for (int y = 0; y < LED_HEIGHT; y++) {
        draw_pixel(x, y, c);
    }
}

int draw_glyph(int g, int x, uint8_t fg, uint8_t bg)
{
    // ASCII?
    if (g > 127) {
        return x;
    }

    // draw glyph
    unsigned char aa = 0;
    for (int col = 0; col < 5; col++) {
        unsigned char a = font[g][col];

        // skip repeating space
        if ((aa == 0) && (a == 0)) {
            continue;
        }
        aa = a;

        // draw column
        for (int y = 0; y < 7; y++) {
            draw_pixel(x, y, (a & 1) ? fg : bg);
            a >>= 1;
        }
        x++;
    }

    // draw space until next character
    if (aa != 0) {
        draw_vline(x, bg);
        x++;
    }

    return x;
}

int draw_text(const char *text, int x, uint8_t fg, uint8_t bg)
{
    for (size_t i = 0; i < strlen(text); i++) {
        x = draw_glyph(text[i], x, fg, bg);
    }
    return x;
}

