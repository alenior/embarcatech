#include "display.h"
#include "config.h"
#include "ssd1306.h"
#include "wifi.h"
#include "buttons.h"
#include "joystick.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
    DISPLAY_MODE_STATUS = 0,
    DISPLAY_MODE_MESSAGE
} display_mode_t;

static char current[32] = "INICIANDO";
static bool dirty = true;
static absolute_time_t last_refresh = {0};
static absolute_time_t last_scroll = {0};
static absolute_time_t last_invert = {0};
static display_mode_t mode = DISPLAY_MODE_STATUS;
static bool msg_invert = false;
static int msg_offset = 0;

static const char *msg_words[] = {
    "EMBARCATECH",
    "2026",
    "PROJETO",
    "FINAL",
    "OBRIGADO",
    "POR",
    "ASSISTIR!"};

#define MSG_WORDS_COUNT ((int)(sizeof(msg_words) / sizeof(msg_words[0])))

static void format_uptime(char *out, size_t out_sz)
{
    uint64_t ms = to_ms_since_boot(get_absolute_time());
    uint32_t total_s = (uint32_t)(ms / 1000u);
    uint32_t min = total_s / 60u;
    uint32_t sec = total_s % 60u;
    snprintf(out, out_sz, "%02u:%02u", min, sec);
}

static void render_status_screen(void)
{
    char line_status[32];
    char line_wifi[20];
    char line_device[28];

    snprintf(line_status, sizeof(line_status), "St: %.18s", current);
    snprintf(line_wifi, sizeof(line_wifi), "WiFi: %s", wifi_is_connected() ? "CONECTADO" : "OFFLINE");

    char up[12];
    format_uptime(up, sizeof(up));
    snprintf(line_device, sizeof(line_device), "Dev: %.16s", DEVICE_ID);

    ssd1306_clear();
    ssd1306_draw_string(18, 0, "BITDOGLAB ALARME");
    ssd1306_draw_string(0, 20, line_status);
    ssd1306_draw_string(0, 38, line_wifi);
    ssd1306_draw_string(0, 54, line_device);
    ssd1306_show();
}

static void render_message_screen(void)
{
    ssd1306_clear();
    ssd1306_draw_string(5, 0, "MENSAGEM IMPORTANTE:");

    for (int i = 0; i < 3; i++)
    {
        int idx = msg_offset + i;
        if (idx < MSG_WORDS_COUNT)
            ssd1306_draw_string(0, 16 + i * 16, msg_words[idx]);
    }

    ssd1306_show();
}

static void display_render(void)
{
    ssd1306_set_invert(mode == DISPLAY_MODE_MESSAGE && msg_invert);

    if (mode == DISPLAY_MODE_MESSAGE)
        render_message_screen();
    else
        render_status_screen();
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
    if (buttons_back())
    {
        mode = DISPLAY_MODE_MESSAGE;
        dirty = true;
    }

    if (mode == DISPLAY_MODE_MESSAGE)
    {
        if (joystick_button_pressed())
        {
            mode = DISPLAY_MODE_STATUS;
            dirty = true;
        }
        else
        {
            uint16_t y = joystick_read_x();
            if (absolute_time_diff_us(last_scroll, get_absolute_time()) > 180000)
            {
                if (y > 3000 && msg_offset > 0)
                {
                    msg_offset--;
                    last_scroll = get_absolute_time();
                    dirty = true;
                }
                else if (y < 1000 && msg_offset < (MSG_WORDS_COUNT - 3))
                {
                    msg_offset++;
                    last_scroll = get_absolute_time();
                    dirty = true;
                }
            }

            uint16_t x = joystick_read_y();
            if (absolute_time_diff_us(last_invert, get_absolute_time()) > 250000)
            {
                if (x < 1000 || x > 3000)
                {
                    msg_invert = !msg_invert;
                    last_invert = get_absolute_time();
                    dirty = true;
                }
            }
        }
    }

    bool periodic = absolute_time_diff_us(last_refresh, get_absolute_time()) >= 1000000;
    if (!dirty && !periodic)
        return;

    dirty = false;
    last_refresh = get_absolute_time();
    display_render();
}