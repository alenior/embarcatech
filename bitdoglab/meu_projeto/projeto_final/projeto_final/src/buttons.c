#include <stdio.h>
#include "buttons.h"
#include "config.h"
#include "pico/stdlib.h"

static absolute_time_t last_press_a;
static absolute_time_t last_press_b;
static bool last_raw_a = true;
static bool last_raw_b = true;

void buttons_init()
{
    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);

    gpio_init(BTN_B);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_B);

    last_press_a = get_absolute_time();
    last_press_b = get_absolute_time();
    printf("Buttons init\n");
}

bool button_a_pressed()
{
    bool raw = gpio_get(BTN_A);
    bool pressed = false;

    if (last_raw_a && !raw && absolute_time_diff_us(last_press_a, get_absolute_time()) > 80000)
    {
        last_press_a = get_absolute_time();
        pressed = true;
    }

    last_raw_a = raw;
    return pressed;
}

bool button_b_pressed()
{
    bool raw = gpio_get(BTN_B);
    bool pressed = false;

    if (last_raw_b && !raw && absolute_time_diff_us(last_press_b, get_absolute_time()) > 80000)
    {
        last_press_b = get_absolute_time();
        pressed = true;
    }

    last_raw_b = raw;
    return pressed;
}

bool buttons_ok(void)
{
    return button_a_pressed();
}

bool buttons_back(void)
{
    return button_b_pressed();
}