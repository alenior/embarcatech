#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/watchdog.h"

#include "wifi.h"
#include "firebase.h"
#include "control.h"
#include "pir.h"
#include "audio.h"
#include "buttons.h"
#include "joystick.h"
#include "display.h"
#include "alarm.h"

int main(void)
{
    stdio_init_all();
    sleep_ms(1200);

    printf("BOOT OK\n");
    printf("Conectando WiFi...\n");
    printf("Dica: Pico W suporta apenas rede 2.4GHz\n");

    if (cyw43_arch_init_with_country(CYW43_COUNTRY_BRAZIL))
    {
        printf("ERRO: cyw43 init falhou\n");
        return 1;
    }

    wifi_init();
    if (wifi_is_connected())
        printf("Main -> WiFi OK\n");
    else
        printf("Main -> WiFi inicial não conectado (reconnect em background)\n");

    control_init();
    firebase_init();

    pir_init();
    printf("Main -> PIR OK\n");

    audio_init();
    printf("Main -> Audio OK\n");

    buttons_init();
    printf("Main -> Buttons OK\n");

    joystick_init();
    printf("Main -> Joystick OK\n");

    display_init();
    printf("Main -> Display OK\n");

    alarm_init();
    printf("Main -> Alarm OK\n");

    watchdog_enable(4000, 1);
    printf("Main -> Watchdog OK\n");

    while (true)
    {
        wifi_task();
        firebase_fetch_control();
        firebase_task();
        alarm_task();
        display_task();

        watchdog_update();
        sleep_ms(10);
    }

    return 0;
}