#include <stdint.h>

#include "framebuffer.h"

void draw_init(uint8_t *framebuffer);
bool draw_pixel(int x, int y, uint8_t p);
int draw_text(const char *text, int x, uint8_t fg, uint8_t bg);


