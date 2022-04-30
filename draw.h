#include <stdint.h>

#include "framebuffer.h"

void draw_init(uint8_t *framebuffer, double gamma);
void draw_flip(bool horizontal, bool vertical);

void draw_clear(void);
bool draw_pixel(int x, int y, uint8_t p);
void draw_vline(int x, uint8_t c);
int draw_text(const char *text, int x, uint8_t fg, uint8_t bg);


