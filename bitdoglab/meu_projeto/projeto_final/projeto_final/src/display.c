#include "display.h"
#include "config.h"
#include "ssd1306.h"
#include "wifi.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static char current[32] = "INICIANDO";
static bool dirty = true;
static absolute_time_t last_refresh = {0};

static void format_uptime(char *out, size_t out_sz)
{
    uint64_t ms = to_ms_since_boot(get_absolute_time());
    uint32_t total_s = (uint32_t)(ms / 1000u);
    uint32_t min = total_s / 60u;
    uint32_t sec = total_s % 60u;
    snprintf(out, out_sz, "%02u:%02u", min, sec);
}

static void display_render(void)
{
    char line_status[32];
    char line_wifi[20];
    char line_device[28];

    snprintf(line_status, sizeof(line_status), "Status: %.18s", current);
    snprintf(line_wifi, sizeof(line_wifi), "WiFi: %s", wifi_is_connected() ? "CONECTADO" : "OFFLINE");

    char up[12];
    format_uptime(up, sizeof(up));
    snprintf(line_device, sizeof(line_device), "Disp.: %.16s", DEVICE_ID);

    ssd1306_clear();
    ssd1306_draw_string(20, 0, "BITDOGLAB ALARME");
    ssd1306_draw_string(0, 18, line_status);
    ssd1306_draw_string(0, 34, line_wifi);
    ssd1306_draw_string(0, 50, line_device);
    ssd1306_show();
}

void display_init()
{
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
    display_render();

    printf("Display init\n");
}

void display_set_status(const char *status)
{
    snprintf(current, sizeof(current), "%s", status);
    dirty = true;
}

void display_task()
{
    bool periodic = absolute_time_diff_us(last_refresh, get_absolute_time()) >= 1000000;
    if (!dirty && !periodic)
        return;

    dirty = false;
    last_refresh = get_absolute_time();
    display_render();
}