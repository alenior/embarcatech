#include <stdio.h>
#include "buttons.h"
#include "pico/stdlib.h"

#define BTN_A 5
#define BTN_B 6

static absolute_time_t last_press_a;
static absolute_time_t last_press_b;

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
    if (!gpio_get(BTN_A))
    {
        if (absolute_time_diff_us(last_press_a, get_absolute_time()) > 200000)
        {
            last_press_a = get_absolute_time();
            return true;
        }
    }
    return false;
}

bool button_b_pressed()
{
    if (!gpio_get(BTN_B))
    {
        if (absolute_time_diff_us(last_press_b, get_absolute_time()) > 200000)
        {
            last_press_b = get_absolute_time();
            return true;
        }
    }
    return false;
}

bool buttons_ok(void)
{
    return button_a_pressed();
}

bool buttons_back(void)
{
    return button_b_pressed();
}