#include "wifi.h"
#include "config.h"

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "pico/cyw43_arch.h"

#include <stdio.h>
#include <string.h>

static bool connected = false;
static absolute_time_t last_retry = {0};

typedef struct
{
    uint32_t auth;
    const char *name;
} wifi_auth_t;

typedef struct
{
    bool complete;
    bool target_found;
    int target_rssi;
} scan_env_t;

static const wifi_auth_t auth_modes[] = {
    {CYW43_AUTH_WPA2_AES_PSK, "WPA2_AES"},
    {CYW43_AUTH_WPA2_MIXED_PSK, "WPA2_MIXED"},
};

static int wifi_scan_cb(void *env, const cyw43_ev_scan_result_t *result)
{
    scan_env_t *scan = (scan_env_t *)env;

    if (result == NULL)
    {
        scan->complete = true;
        return 0;
    }

    printf("SCAN ssid=%.*s rssi=%d ch=%u auth=%u\n",
           result->ssid_len,
           result->ssid,
           result->rssi,
           result->channel,
           result->auth_mode);

    size_t target_len = strlen(WIFI_SSID);
    if (result->ssid_len == target_len && strncmp((const char *)result->ssid, WIFI_SSID, target_len) == 0)
    {
        scan->target_found = true;
        scan->target_rssi = result->rssi;
    }

    return 0;
}

static void wifi_scan_nearby(void)
{
    cyw43_wifi_scan_options_t options = {0};
    scan_env_t scan = {
        .complete = false,
        .target_found = false,
        .target_rssi = -127,
    };

    int err = cyw43_wifi_scan(&cyw43_state, &options, &scan, wifi_scan_cb);
    if (err != 0)
    {
        printf("SCAN erro ao iniciar (%d)\n", err);
        return;
    }

    absolute_time_t start = get_absolute_time();
    while (!scan.complete && absolute_time_diff_us(start, get_absolute_time()) < 10000000)
    {
        cyw43_arch_poll();
        sleep_ms(50);
    }

    if (scan.target_found)
        printf("SCAN alvo encontrado: %s (rssi=%d)\n", WIFI_SSID, scan.target_rssi);
    else
        printf("SCAN alvo NAO encontrado: %s\n", WIFI_SSID);
}

static void wifi_print_ip(void)
{
    const ip4_addr_t *ip = netif_ip4_addr(netif_default);
    if (ip != NULL)
        printf("WiFi IP: %s\n", ip4addr_ntoa(ip));
}

static bool wifi_try_connect_blocking(void)
{
    const int timeout_ms = 12000;

    printf("WiFi SSID alvo: %s\n", WIFI_SSID);
    wifi_scan_nearby();

    for (size_t i = 0; i < sizeof(auth_modes) / sizeof(auth_modes[0]); i++)
    {
        const wifi_auth_t mode = auth_modes[i];

        printf("WiFi -> tentativa (auth=%s)\n", mode.name);
        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);

        int rc = cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASS,
            mode.auth,
            timeout_ms);

        if (rc == 0)
        {
            printf("WiFi conectado com %s\n", mode.name);
            wifi_print_ip();
            return true;
        }

        printf("WiFi falhou com %s (rc=%d)\n", mode.name, rc);
    }

    return false;
}

void wifi_init()
{
    printf("Conectando WiFi...\n");
    printf("Dica: Pico W suporta apenas rede 2.4GHz\n");

    cyw43_arch_enable_sta_mode();

    connected = wifi_try_connect_blocking();
    last_retry = get_absolute_time();

    if (!connected)
        printf("WiFi init sem conexão, ficará tentando em background\n");

    printf("WiFi init\n");
}

void wifi_task()
{
    cyw43_arch_poll();

    int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    bool has_ip = false;
    if (netif_default != NULL && netif_is_up(netif_default))
    {
        const ip4_addr_t *ip = netif_ip4_addr(netif_default);
        has_ip = (ip != NULL) && !ip4_addr_isany_val(*ip);
    }

    connected = has_ip;

    static absolute_time_t last_log = {0};
    if (absolute_time_diff_us(last_log, get_absolute_time()) > 3000000)
    {
        last_log = get_absolute_time();
        printf("WiFi status=%d has_ip=%d\n", status, connected ? 1 : 0);
    }

    if (connected)
    {
        last_retry = get_absolute_time();
        return;
    }

    if (absolute_time_diff_us(last_retry, get_absolute_time()) > 20000000)
    {
        last_retry = get_absolute_time();
        printf("WiFi reconectando...\n");
        connected = wifi_try_connect_blocking();
    }
}

bool wifi_is_connected()
{
    return connected;
}