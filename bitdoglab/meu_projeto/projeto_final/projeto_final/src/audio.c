#include <stdio.h>

#include "audio.h"
#include "config.h"
#include "hardware/adc.h"
#include "pico/time.h"

static uint16_t baseline = 0;
static bool initialized = false;
static absolute_time_t last_trigger = {0};
static absolute_time_t arm_grace_until = {0};

#define SOUND_THRESHOLD 80
#define COOLDOWN_US 1200000
#define SAMPLE_INTERVAL_US 5000
#define BASELINE_SAMPLES 32
#define ARM_GRACE_US 3000000

void audio_init(void)
{
    adc_init();
    adc_gpio_init(AUDIO_PIN);
    audio_rearm();
    printf("Audio init\n");
}

void audio_rearm(void)
{
    initialized = false;
    baseline = 0;
    arm_grace_until = make_timeout_time_us(ARM_GRACE_US);
}

bool audio_detect(void)
{
    static absolute_time_t last_read = {0};

    if (absolute_time_diff_us(last_read, get_absolute_time()) < SAMPLE_INTERVAL_US)
    {
        return false;
    }

    last_read = get_absolute_time();

    if (absolute_time_diff_us(get_absolute_time(), arm_grace_until) > 0)
    {
        return false;
    }

    // garante canal correto mesmo se outro módulo usar o ADC
    adc_select_input(AUDIO_PIN - 26);

    uint16_t v = adc_read();
    if (v > 4095)
    {
        return false;
    }

    if (!initialized)
    {
        uint32_t sum = 0;
        for (int i = 0; i < BASELINE_SAMPLES; i++)
        {
            sum += adc_read();
            sleep_us(200);
        }

        baseline = (uint16_t)(sum / BASELINE_SAMPLES);
        initialized = true;
        return false;
    }

    baseline = (uint16_t)((baseline * 15 + v) / 16);

    int diff = (int)v - (int)baseline;
    if (diff < 0)
    {
        diff = -diff;
    }

    if (diff > SOUND_THRESHOLD)
    {
        if (absolute_time_diff_us(last_trigger, get_absolute_time()) < COOLDOWN_US)
        {
            return false;
        }

        last_trigger = get_absolute_time();
        return true;
    }

    return false;
}