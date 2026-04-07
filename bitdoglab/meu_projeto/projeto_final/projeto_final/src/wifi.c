#include "wifi.h"
#include "config.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>

static bool connected = false;
static absolute_time_t last_attempt;

void wifi_init()
{
    printf("Conectando WiFi...\n");

    cyw43_arch_enable_sta_mode();

    cyw43_arch_wifi_connect_async(
        WIFI_SSID,
        WIFI_PASS,
        CYW43_AUTH_WPA2_MIXED_PSK);

    last_attempt = get_absolute_time();
    printf("WiFi init\n");
}

void wifi_task()
{
    cyw43_arch_poll();

    int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);

    switch (status)
    {
    case CYW43_LINK_UP:
        if (!connected)
        {
            printf("WiFi conectado\n");
            connected = true;
        }
        return;

    case CYW43_LINK_JOIN:
    case CYW43_LINK_NOIP:
        // ainda conectando → NÃO interfere
        return;

    default:
        connected = false;
        break;
    }

    if (absolute_time_diff_us(last_attempt, get_absolute_time()) > 10000000)
    {
        printf("Reconectando WiFi...\n");

        cyw43_arch_wifi_connect_async(
            WIFI_SSID,
            WIFI_PASS,
            CYW43_AUTH_WPA2_MIXED_PSK);

        last_attempt = get_absolute_time();
    }

    printf("WiFi status: %d\n", status);
}

bool wifi_is_connected()
{
    return connected;
}