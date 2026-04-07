#include "wifi.h"
#include "config.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>

static bool connected = false;
static absolute_time_t last_attempt;
static absolute_time_t last_status_log;

static const char *wifi_status_str(int status)
{
    switch (status)
    {
    case CYW43_LINK_DOWN:
        return "DOWN";
    case CYW43_LINK_JOIN:
        return "JOIN";
    case CYW43_LINK_NOIP:
        return "NOIP";
    case CYW43_LINK_UP:
        return "UP";
    case CYW43_LINK_FAIL:
        return "FAIL";
    case CYW43_LINK_NONET:
        return "NONET";
    case CYW43_LINK_BADAUTH:
        return "BADAUTH";
    default:
        return "UNKNOWN";
    }
}

static void wifi_start_connect(void)
{
    cyw43_arch_wifi_connect_async(
        WIFI_SSID,
        WIFI_PASS,
        CYW43_AUTH_WPA2_MIXED_PSK);

    last_attempt = get_absolute_time();
    printf("WiFi -> tentativa de conexão iniciada\n");
}

void wifi_init()
{
    printf("Conectando WiFi...\n");

    cyw43_arch_enable_sta_mode();
    wifi_start_connect();
    last_status_log = get_absolute_time();

    printf("WiFi init\n");
}

void wifi_task()
{
    cyw43_arch_poll();

    int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);

    if (absolute_time_diff_us(last_status_log, get_absolute_time()) > 2000000)
    {
        last_status_log = get_absolute_time();
        printf("WiFi status=%s (%d)\n", wifi_status_str(status), status);
    }

    if (status == CYW43_LINK_UP)
    {
        if (!connected)
        {
            printf("WiFi conectado\n");
            connected = true;
        }
        return;
    }

    connected = false;

    // Se ficar preso em JOIN/NOIP por muito tempo, reinicia tentativa.
    if (absolute_time_diff_us(last_attempt, get_absolute_time()) > 15000000)
    {
        printf("WiFi reconectando (status=%s)\n", wifi_status_str(status));
        wifi_start_connect();
    }
}

bool wifi_is_connected()
{
    return connected;
}