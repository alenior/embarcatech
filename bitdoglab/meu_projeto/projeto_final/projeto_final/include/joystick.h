#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdbool.h>
#include <stdint.h>

void joystick_init(void);
int joystick_get_digit(void);
uint16_t joystick_read_x(void);
uint16_t joystick_read_y(void);
bool joystick_button_pressed(void);

#endif