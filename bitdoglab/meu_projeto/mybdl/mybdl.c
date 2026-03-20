#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

#include "ssd1306.h"

#define OLED_SDA 14
#define OLED_SCL 15

#define JOY_X 26
#define JOY_Y 27
#define JOY_BTN 22

#define BOTAO_A 5

#define WIFI_TIMEOUT_MS 30000
#define MAX_NETWORKS 20

#define JOY_CENTER 2048
#define JOY_DEAD 600

#define KB_ROWS 6
#define KB_COLS 8

volatile bool wifi_scan_complete = false;

typedef enum
{
    STATE_SCAN,
    STATE_LIST,
    STATE_PASSWORD,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_ERROR

} app_state_t;

typedef struct
{
    char ssid[33];
    int rssi;

} wifi_network_t;

wifi_network_t networks[MAX_NETWORKS];

int network_count = 0;
int selected_network = 0;
int scroll_offset = 0;

char password[64];
int pass_len = 0;

app_state_t state = STATE_SCAN;

/* ====================== TECLADO 2D ====================== */

const char keyboard[KB_ROWS][KB_COLS] =
    {
        {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'},
        {'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P'},
        {'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X'},
        {'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f'},
        {'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n'},
        {'o', 'p', 'q', 'r', 's', 't', 'u', 'v'}};

const char keyboard2[KB_ROWS][KB_COLS] =
    {
        {'w', 'x', 'y', 'z', '0', '1', '2', '3'},
        {'4', '5', '6', '7', '8', '9', '*', '#'},
        {'&', '!', '%', '?', '-', '+', '_', '<'},
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', 'O'} // O = OK
};

int key_row = 0;
int key_col = 0;
bool keyboard_page = false;

/* ====================== OLED ====================== */

void oled_clear()
{
    ssd1306_clear();
}

void oled_show()
{
    ssd1306_show();
}

/* ====================== WIFI ====================== */

bool ssid_exists(char *ssid)
{
    for (int i = 0; i < network_count; i++)
        if (strcmp(networks[i].ssid, ssid) == 0)
            return true;

    return false;
}

static int scan_callback(void *env, const cyw43_ev_scan_result_t *result)
{
    if (!result)
        return 0;

    char ssid[33];

    memcpy(ssid, result->ssid, result->ssid_len);
    ssid[result->ssid_len] = 0;

    if (ssid_exists(ssid))
        return 0;

    if (network_count >= MAX_NETWORKS)
    {
        wifi_scan_complete = true;
        return 0;
    }

    strcpy(networks[network_count].ssid, ssid);
    networks[network_count].rssi = result->rssi;

    network_count++;

    return 0;
}

void sort_networks()
{
    for (int i = 0; i < network_count - 1; i++)
        for (int j = i + 1; j < network_count; j++)
        {
            if (networks[j].rssi > networks[i].rssi)
            {
                wifi_network_t tmp = networks[i];
                networks[i] = networks[j];
                networks[j] = tmp;
            }
        }
}

void scan_wifi()
{
    network_count = 0;
    wifi_scan_complete = false;

    cyw43_wifi_scan_options_t scan_options = {0};

    oled_clear();
    ssd1306_draw_string(0, 0, "Escaneando");
    ssd1306_draw_string(0, 16, "redes WiFi");
    oled_show();

    cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_callback);

    absolute_time_t timeout = make_timeout_time_ms(5000);

    while (!wifi_scan_complete)
    {
        sleep_ms(10);

        if (absolute_time_diff_us(get_absolute_time(), timeout) < 0)
            wifi_scan_complete = true;
    }

    if (network_count == 0)
    {
        state = STATE_SCAN;
        return;
    }

    sort_networks();

    selected_network = 0;
    scroll_offset = 0;

    state = STATE_LIST;
}

/* ====================== RSSI ====================== */

int rssi_to_bars(int rssi)
{
    if (rssi > -55)
        return 4;
    if (rssi > -65)
        return 3;
    if (rssi > -75)
        return 2;
    if (rssi > -85)
        return 1;
    return 0;
}

void draw_signal_bars(int x, int y, int bars)
{
    for (int i = 0; i < bars; i++)
    {
        int h = (i + 1) * 3;

        for (int j = 0; j < h; j++)
        {
            ssd1306_draw_pixel(x + i * 3, y - j);
            ssd1306_draw_pixel(x + i * 3 + 1, y - j);
        }
    }
}

/* ====================== MENU REDES ====================== */

void draw_networks()
{
    oled_clear();

    for (int i = 0; i < 4; i++)
    {
        int idx = scroll_offset + i;

        if (idx >= network_count)
            break;

        char line[20];

        sprintf(line, "%c %.9s",
                idx == selected_network ? '>' : ' ',
                networks[idx].ssid);

        ssd1306_draw_string(0, i * 16, line);

        int bars = rssi_to_bars(networks[idx].rssi);

        draw_signal_bars(88, (i * 16) + 12, bars);
    }

    char info[16];

    sprintf(info, "%d/%d", selected_network + 1, network_count);

    ssd1306_draw_string(110, 0, info);

    oled_show();
}

/* ====================== JOYSTICK ====================== */

int joystick_vertical()
{
    adc_select_input(0);
    int y = adc_read();

    if (y > JOY_CENTER + JOY_DEAD)
        return -1;
    if (y < JOY_CENTER - JOY_DEAD)
        return 1;

    return 0;
}

int joystick_horizontal()
{
    adc_select_input(1);
    int x = adc_read();

    if (x > JOY_CENTER + JOY_DEAD)
        return 1;
    if (x < JOY_CENTER - JOY_DEAD)
        return -1;

    return 0;
}

bool joystick_pressed()
{
    return !gpio_get(JOY_BTN);
}

bool buttonA_pressed()
{
    return !gpio_get(BOTAO_A);
}

void handle_network_menu()
{
    draw_networks();

    int dir = joystick_vertical();

    int visible_lines = 4;
    int max_scroll = network_count - visible_lines;

    if (max_scroll < 0)
        max_scroll = 0;

    if (dir == 1 && selected_network < network_count - 1)
    {
        selected_network++;

        if (selected_network >= scroll_offset + visible_lines &&
            scroll_offset < max_scroll)
        {
            scroll_offset++;
        }

        sleep_ms(180);
    }

    if (dir == -1 && selected_network > 0)
    {
        selected_network--;

        if (selected_network < scroll_offset)
        {
            scroll_offset--;
        }

        sleep_ms(180);
    }

    if (joystick_pressed())
    {
        sleep_ms(300);
        state = STATE_PASSWORD;
    }
}

/* ====================== SENHA ====================== */

void draw_password_screen()
{
    oled_clear();

    ssd1306_draw_string(0, 0, "Senha:");

    char masked[64];

    for (int i = 0; i < pass_len; i++)
        masked[i] = '*';
    masked[pass_len] = 0;

    ssd1306_draw_string(0, 12, masked);

    const char (*kb)[KB_COLS] = keyboard_page ? keyboard2 : keyboard;

    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 8; c++)
        {
            char buf[4];

            char k = kb[r][c];

            if (r == key_row && c == key_col)
                sprintf(buf, "[%c]", k);
            else
                sprintf(buf, " %c ", k);

            ssd1306_draw_string(c * 16, r * 16 + 24, buf);
        }

    oled_show();
}

void handle_password()
{
    draw_password_screen();

    int dx = joystick_horizontal();
    int dy = joystick_vertical();

    key_col += dx;
    key_row += dy;

    if (key_col < 0)
        key_col = 0;
    if (key_col >= KB_COLS)
        key_col = KB_COLS - 1;

    if (key_row < 0)
        key_row = 0;
    if (key_row >= 3)
        key_row = 2;

    const char (*kb)[KB_COLS] = keyboard_page ? keyboard2 : keyboard;

    char key = kb[key_row][key_col];

    if (joystick_pressed())
    {
        sleep_ms(200);

        if (key == '<')
        {
            if (pass_len > 0)
            {
                pass_len--;
                password[pass_len] = 0;
            }
        }
        else if (key == 'O')
        {
            state = STATE_CONNECTING;
        }
        else
        {
            if (pass_len < 63)
            {
                password[pass_len++] = key;
                password[pass_len] = 0;
            }
        }
    }

    if (buttonA_pressed())
    {
        keyboard_page = !keyboard_page;
        sleep_ms(250);
    }
}

/* ====================== WIFI ====================== */

void connect_wifi()
{
    oled_clear();
    ssd1306_draw_string(0, 0, "Conectando...");
    ssd1306_draw_string(0, 16, networks[selected_network].ssid);
    oled_show();

    int result = cyw43_arch_wifi_connect_timeout_ms(
        networks[selected_network].ssid,
        password,
        CYW43_AUTH_WPA2_AES_PSK,
        WIFI_TIMEOUT_MS);

    if (result == 0)
        state = STATE_CONNECTED;
    else
        state = STATE_ERROR;
}

void screen_connected()
{
    oled_clear();
    ssd1306_draw_string(0, 0, "WiFi conectado");
    ssd1306_draw_string(0, 16, networks[selected_network].ssid);
    oled_show();
}

void screen_error()
{
    oled_clear();
    ssd1306_draw_string(0, 0, "Falha WiFi");
    ssd1306_draw_string(0, 16, "tente novamente");
    oled_show();
}

/* ====================== MAIN ====================== */

int main()
{
    stdio_init_all();

    adc_init();

    adc_gpio_init(JOY_X);
    adc_gpio_init(JOY_Y);

    gpio_init(JOY_BTN);
    gpio_set_dir(JOY_BTN, false);
    gpio_pull_up(JOY_BTN);

    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, false);
    gpio_pull_up(BOTAO_A);

    i2c_init(i2c1, 400000);

    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);

    ssd1306_init();

    oled_clear();
    ssd1306_draw_string(0, 0, "BitDogLab");
    ssd1306_draw_string(0, 16, "Inicializando");
    oled_show();

    sleep_ms(1000);

    if (cyw43_arch_init())
        return -1;

    cyw43_arch_enable_sta_mode();

    while (true)
    {
        switch (state)
        {
        case STATE_SCAN:
            scan_wifi();
            break;
        case STATE_LIST:
            handle_network_menu();
            break;
        case STATE_PASSWORD:
            handle_password();
            break;
        case STATE_CONNECTING:
            connect_wifi();
            break;
        case STATE_CONNECTED:
            screen_connected();
            break;
        case STATE_ERROR:
            screen_error();
            break;
        }

        sleep_ms(50);
    }
}