#include <stdio.h>
#include "joystick.h"
#include "config.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/time.h"

static int current_digit = 0;
static absolute_time_t last_move = {0};
static absolute_time_t joy_btn_last = {0};

void joystick_init()
{
    adc_init();

    adc_gpio_init(JOY_X);
    adc_gpio_init(JOY_Y);

    gpio_init(JOY_BTN);
    gpio_set_dir(JOY_BTN, GPIO_IN);
    gpio_pull_up(JOY_BTN);

    printf("Joystick init\n");
}

uint16_t joystick_read_x(void)
{
    adc_select_input(JOY_X - 26);
    return adc_read();
}

uint16_t joystick_read_y(void)
{
    adc_select_input(JOY_Y - 26);
    return adc_read();
}

bool joystick_button_pressed(void)
{
    if (!gpio_get(JOY_BTN))
    {
        if (absolute_time_diff_us(joy_btn_last, get_absolute_time()) > 180000)
        {
            joy_btn_last = get_absolute_time();
            return true;
        }
    }

    return false;
}

int joystick_get_digit()
{
    uint16_t x = joystick_read_x();

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