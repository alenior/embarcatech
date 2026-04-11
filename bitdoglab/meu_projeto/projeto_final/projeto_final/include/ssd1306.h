#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>

void ssd1306_init();
void ssd1306_clear();
void ssd1306_update();
void ssd1306_draw_pixel(int x, int y);

void ssd1306_draw_char(int x, int y, char c);
void ssd1306_draw_string(int x, int y, const char *str);
void ssd1306_set_invert(bool invert);

#define ssd1306_show() ssd1306_update()

#endif