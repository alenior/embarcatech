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

typedef enum
{
    TRIGGER_NONE,
    TRIGGER_MOTION,
    TRIGGER_SOUND
} trigger_type_t;

static state_t state = ARMED;
static state_t last = -1;
static trigger_type_t trigger_type = TRIGGER_NONE;

static void set_leds_for_state(state_t s)
{
    gpio_put(LED_R, s == TRIGGERED);
    gpio_put(LED_G, s == DISARMED);
    gpio_put(LED_B, s == ARMED);
}

static const char *status_text(void)
{
    if (state == DISARMED)
        return "DESARMADO";

    if (state == ARMED)
        return "ARMADO";

    if (trigger_type == TRIGGER_MOTION)
        return "ALERTA - MOVIMENTO";

    if (trigger_type == TRIGGER_SOUND)
        return "ALERTA - SOM";

    return "ALERTA";
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
    display_set_status(status_text());
    firebase_set_status(status_text());
    printf("Alarm init\n");
}

void alarm_task()
{
    static absolute_time_t last_run = {0};

    if (absolute_time_diff_us(last_run, get_absolute_time()) < 50000)
        return;

    last_run = get_absolute_time();

    bool pir = pir_get();
    bool sound = audio_detect();

    switch (state)
    {
    case DISARMED:
        if (buttons_ok())
            state = ARMED;
        break;

    case ARMED:
        if (pir)
        {
            trigger_type = TRIGGER_MOTION;
            state = TRIGGERED;
        }
        else if (sound)
        {
            trigger_type = TRIGGER_SOUND;
            state = TRIGGERED;
        }
        break;

    case TRIGGERED:
        if (buttons_ok())
        {
            state = DISARMED;
            trigger_type = TRIGGER_NONE;
        }
        break;
    }

    if (state != last)
    {
        last = state;
        set_leds_for_state(state);

        const char *status = status_text();
        display_set_status(status);
        firebase_set_status(status);

        if (state == DISARMED)
            firebase_log("DESARMADO", "SYSTEM");
        else if (state == ARMED)
            firebase_log("ARMADO", "SYSTEM");
        else if (trigger_type == TRIGGER_MOTION)
            firebase_log("ALERTA - MOVIMENTO", "MOTION");
        else if (trigger_type == TRIGGER_SOUND)
            firebase_log("ALERTA - SOM", "SOUND");
        else
            firebase_log("ALERTA", "ALARM");
    }
}