#include "alarm.h"
#include "display.h"
#include "firebase.h"
#include "pir.h"
#include "audio.h"
#include "buttons.h"
#include "config.h"

#include <stdio.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

typedef enum
{
    DISARMED,
    ARMED,
    TRIGGERED
} state_t;

static state_t state = ARMED;
static state_t last = -1;

static void set_leds_for_state(state_t s)
{
    gpio_put(LED_R, s == TRIGGERED);
    gpio_put(LED_G, s == DISARMED);
    gpio_put(LED_B, s == ARMED);
}

void alarm_init()
{
    gpio_init(LED_R);
    gpio_init(LED_G);
    gpio_init(LED_B);

    gpio_set_dir(LED_R, GPIO_OUT);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_set_dir(LED_B, GPIO_OUT);

    set_leds_for_state(state);
    printf("Alarm init\n");
}

void alarm_task()
{
    static absolute_time_t last_run = {0};

    if (absolute_time_diff_us(last_run, get_absolute_time()) < 50000)
        return;

    last_run = get_absolute_time();

    static absolute_time_t last_log_time = {0};

    if (state != last)
    {
        set_leds_for_state(state);

        if (absolute_time_diff_us(last_log_time, get_absolute_time()) > 500000)
        {
            last_log_time = get_absolute_time();
            last = state;

            if (state == DISARMED)
                firebase_log("DISARMED", "SYSTEM");

            if (state == ARMED)
                firebase_log("ARMED", "SYSTEM");

            if (state == TRIGGERED)
                firebase_log("TRIGGERED", "ALARM");
        }
    }

    bool pir = pir_get();
    bool sound = audio_detect();

    static state_t last_display = -1;

    if (state != last_display)
    {
        last_display = state;

        if (state == DISARMED)
            display_set_status("DESARMADO");

        if (state == ARMED)
            display_set_status("ARMADO");

        if (state == TRIGGERED)
            display_set_status("ALERTA");
    }

    switch (state)
    {
    case DISARMED:
        if (buttons_ok())
            state = ARMED;
        break;

    case ARMED:
        if (pir || sound)
            state = TRIGGERED;
        break;

    case TRIGGERED:
        if (buttons_ok())
            state = DISARMED;
        break;
    }
}