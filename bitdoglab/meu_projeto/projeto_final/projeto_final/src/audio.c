#include <stdio.h>

#include "audio.h"
#include "config.h"
#include "hardware/adc.h"
#include "pico/time.h"

static uint16_t baseline = 0;
static bool initialized = false;
static absolute_time_t last_trigger = {0};

#define SOUND_THRESHOLD 150
#define COOLDOWN_US 2000000

void audio_init(void)
{
    adc_init();
    adc_gpio_init(AUDIO_PIN);
    printf("Audio init\n");
}

bool audio_detect(void)
{
    static bool adc_ready = false;
    static absolute_time_t last_read = {0};

    if (!adc_ready)
    {
        adc_select_input(AUDIO_PIN - 28);
        adc_ready = true;
    }

    // lê no máximo a cada 10ms
    if (absolute_time_diff_us(last_read, get_absolute_time()) < 10000)
    {
        return false;
    }

    last_read = get_absolute_time();

    uint16_t v = adc_read();
    if (v > 4095)
    {
        return false; // sanity check
    }

    if (!initialized)
    {
        baseline = v;
        initialized = true;
        return false;
    }

    baseline = (baseline * 7 + v) / 8;

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