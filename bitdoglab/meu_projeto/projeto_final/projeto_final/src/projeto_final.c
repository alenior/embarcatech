#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/watchdog.h"

#include "wifi.h"
#include "firebase.h"
#include "display.h"
#include "alarm.h"
#include "pir.h"
#include "audio.h"
#include "buttons.h"
#include "joystick.h"
#include "utils.h"

#ifndef CYW43_COUNTRY_BRAZIL
#define CYW43_COUNTRY_BRAZIL CYW43_COUNTRY('B', 'R', 0)
#endif

int main()
{
    stdio_init_all();
    sleep_ms(2000);

    printf("BOOT OK\n");

    if (cyw43_arch_init_with_country(CYW43_COUNTRY_BRAZIL))
    {
        printf("Erro WiFi (init country BR)\n");
        return -1;
    }

    wifi_init();
    printf("Main -> WiFi OK\n");

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

    watchdog_enable(15000, 1);
    printf("Main -> Watchdog OK\n");

    while (true)
    {
        watchdog_update();

        wifi_task();

        firebase_task();

        alarm_task();

        display_task();

        sleep_ms(100);
    }
}