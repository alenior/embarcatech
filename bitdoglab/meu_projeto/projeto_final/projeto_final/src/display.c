#include "display.h"
#include "config.h"
#include "ssd1306.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static char current[32] = "INICIANDO";
static bool dirty = true;

void display_init()
{
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "BitDogLab v6.3");
    ssd1306_draw_string(0, 16, "Alarme");
    ssd1306_draw_string(0, 32, current);
    ssd1306_show();

    printf("Display init\n");
}

void display_set_status(const char *status)
{
    snprintf(current, sizeof(current), "%s", status);
    dirty = true;
}

void display_task()
{
    if (!dirty)
        return;

    dirty = false;

    ssd1306_clear();
    ssd1306_draw_string(0, 0, "Sistema:");
    ssd1306_draw_string(0, 16, current);
    ssd1306_show();
}