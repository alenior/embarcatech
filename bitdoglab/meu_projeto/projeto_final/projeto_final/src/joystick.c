#include <stdio.h>
#include "joystick.h"
#include "config.h"
#include "hardware/adc.h"
#include "pico/time.h"

static int current_digit = 0;
static absolute_time_t last_move = {0};

void joystick_init()
{
    adc_init();

    adc_gpio_init(JOY_X);
    adc_gpio_init(JOY_Y);
    printf("Joystick init\n");
}

int joystick_get_digit()
{
    adc_select_input(JOY_X - 26);

    uint16_t x = adc_read();

    if (absolute_time_diff_us(last_move, get_absolute_time()) > 200000)
    {
        if (x < 1000)
        {
            current_digit = (current_digit + 9) % 10;
            last_move = get_absolute_time();
        }
        else if (x > 3000)
        {
            current_digit = (current_digit + 1) % 10;
            last_move = get_absolute_time();
        }
    }

    return current_digit;
}