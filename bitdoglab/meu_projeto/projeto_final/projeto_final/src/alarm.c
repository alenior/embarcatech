#include "alarm.h"
#include "display.h"
#include "firebase.h"
#include "control.h"
#include "matrix.h"
#include "pir.h"
#include "audio.h"
#include "buttons.h"
#include "config.h"

#include <stdio.h>
#include <stdbool.h>

#include "hardware/gpio.h"
#include "hardware/pwm.h"
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

#define LED_PWM_WRAP 1000

#ifndef BUZZER_PIN
#define BUZZER_PIN 21
#endif

static state_t state = ARMED;
static state_t last = -1;
static trigger_type_t trigger_type = TRIGGER_NONE;

static absolute_time_t state_since = {0};

static void pwm_pin_init(uint pin)
{
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 125.0f);
    pwm_config_set_wrap(&cfg, LED_PWM_WRAP);
    pwm_init(slice, &cfg, true);
    pwm_set_gpio_level(pin, 0);
}

static void led_set(uint16_t r, uint16_t g, uint16_t b)
{
    pwm_set_gpio_level(LED_R, r);
    pwm_set_gpio_level(LED_G, g);
    pwm_set_gpio_level(LED_B, b);
}

static void buzzer_init(void)
{
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 125.0f); // 1 MHz tick
    pwm_config_set_wrap(&cfg, 1000);
    pwm_init(slice, &cfg, true);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

static void buzzer_tone(uint16_t hz)
{
    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN);
    if (hz == 0)
    {
        pwm_set_gpio_level(BUZZER_PIN, 0);
        return;
    }

    uint32_t top = 1000000u / hz;
    if (top < 8)
        top = 8;
    if (top > 65535)
        top = 65535;

    pwm_set_wrap(slice, (uint16_t)(top - 1));
    pwm_set_gpio_level(BUZZER_PIN, (uint16_t)(top / 2));
}

static void update_effects(void)
{
    uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - to_ms_since_boot(state_since);

    if (state == DISARMED)
    {
        uint32_t phase = elapsed_ms % 2000;
        uint16_t level = (phase < 1000) ? (uint16_t)phase : (uint16_t)(2000 - phase);
        led_set(0, level, 0);
        buzzer_tone(0);
        return;
    }

    if (state == ARMED)
    {
        led_set(0, 0, 700);

        bool beep_on = (elapsed_ms < 120) || (elapsed_ms >= 220 && elapsed_ms < 340);
        buzzer_tone(beep_on ? 2400 : 0);
        return;
    }

    // TRIGGERED
    bool blink_on = ((elapsed_ms / 150) % 2) == 0;
    led_set(blink_on ? 1000 : 120, 0, 0);
    buzzer_tone(blink_on ? 1800 : 0);
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

void alarm_init(void)
{
    pwm_pin_init(LED_R);
    pwm_pin_init(LED_G);
    pwm_pin_init(LED_B);
    buzzer_init();
    matrix_init();

    state_since = get_absolute_time();
    matrix_set_pattern(MATRIX_PATTERN_ARMED);
    update_effects();
    display_set_status(status_text());
    firebase_set_status(status_text());
    firebase_log("ARMADO", "SYSTEM");

    printf("Alarm init\n");
}

void alarm_task(void)
{
    // 1) aplica comando remoto pendente (do Firebase control)
    unsigned int cmd_ts = 0;
    control_cmd_t cmd = CONTROL_NONE;
    unsigned int cmd_updated_at = 0;
    if (control_take_pending(&cmd, &cmd_updated_at))
    {
        (void)cmd_updated_at;
        if (cmd == CONTROL_ARM && state == DISARMED)
        {
            state = ARMED;
            trigger_type = TRIGGER_NONE;
            audio_rearm();
        }
        else if (cmd == CONTROL_DISARM)
        {
            state = DISARMED;
            trigger_type = TRIGGER_NONE;
        }
    }

    if (cmd == CONTROL_DISARM && state != DISARMED)
    {
        state = DISARMED;
        trigger_type = TRIGGER_NONE;
        printf("ALARM remote -> DISARMED (ts=%u)\n", cmd_ts);
    }
    else if (cmd == CONTROL_ARM && state == DISARMED)
    {
        state = ARMED;
        printf("ALARM remote -> ARMED (ts=%u)\n", cmd_ts);
    }

    static absolute_time_t last_run = {0};

    if (absolute_time_diff_us(last_run, get_absolute_time()) < 20000)
        return;

    last_run = get_absolute_time();

    // 2) leitura sensores locais
    bool pir = pir_get();
    bool sound = audio_detect();

    switch (state)
    {
    case DISARMED:
        if (buttons_ok())
        {
            state = ARMED;
            audio_rearm();
        }
        break;

    case ARMED:
        if (buttons_ok())
        {
            state = DISARMED;
            trigger_type = TRIGGER_NONE;
        }
        else if (pir)
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
        state_since = get_absolute_time();

        const char *status = status_text();
        display_set_status(status);
        firebase_set_status(status);

        if (state == DISARMED)
        {
            matrix_set_pattern(MATRIX_PATTERN_DISARMED);
            firebase_log("DESARMADO", "SYSTEM");
        }
        else if (state == ARMED)
        {
            matrix_set_pattern(MATRIX_PATTERN_ARMED);
            firebase_log("ARMADO", "SYSTEM");
        }
        else if (trigger_type == TRIGGER_MOTION)
        {
            matrix_set_pattern(MATRIX_PATTERN_TRIGGERED);
            firebase_log("ALERTA - MOVIMENTO", "MOTION");
        }
        else if (trigger_type == TRIGGER_SOUND)
        {
            matrix_set_pattern(MATRIX_PATTERN_TRIGGERED);
            firebase_log("ALERTA - SOM", "SOUND");
        }
        else
        {
            matrix_set_pattern(MATRIX_PATTERN_TRIGGERED);
            firebase_log("ALERTA", "ALARM");
        }
    }
    update_effects();
}