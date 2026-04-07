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

int main()
{
    stdio_init_all();
    sleep_ms(2000);

    printf("BOOT OK\n");

    if (cyw43_arch_init())
    {
        printf("Erro WiFi\n");
        return -1;
    }

    wifi_init();

    pir_init();

    audio_init();

    buttons_init();

    joystick_init();

    display_init();

    alarm_init();

    watchdog_enable(15000, 1);

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