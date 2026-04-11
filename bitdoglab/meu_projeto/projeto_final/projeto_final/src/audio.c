#include <stdio.h>

#include "audio.h"
#include "config.h"
#include "hardware/adc.h"
#include "pico/time.h"

static uint16_t baseline = 0;
static uint16_t noise_floor = 0;
static bool initialized = false;
static absolute_time_t last_trigger = {0};
static absolute_time_t arm_grace_until = {0};

#define SOUND_THRESHOLD_MIN 140
#define NOISE_MULTIPLIER 4
#define NOISE_MARGIN 30
#define TRIGGER_CONSECUTIVE_SAMPLES 3

#define COOLDOWN_US 1500000
#define SAMPLE_INTERVAL_US 5000
#define BASELINE_SAMPLES 64
#define ARM_GRACE_US 4000000

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
    noise_floor = 0;
    arm_grace_until = make_timeout_time_us(ARM_GRACE_US);
}

bool audio_detect(void)
{
    static absolute_time_t last_read = {0};
    static int consecutive_hits = 0;

    if (absolute_time_diff_us(last_read, get_absolute_time()) < SAMPLE_INTERVAL_US)

        return false;

    last_read = get_absolute_time();

    if (absolute_time_diff_us(get_absolute_time(), arm_grace_until) > 0)

        return false;

    adc_select_input(AUDIO_PIN - 26);

    uint16_t v = adc_read();
    if (v > 4095)

        return false;

    if (!initialized)
    {
        uint32_t sum = 0;
        uint16_t min_v = 4095;
        uint16_t max_v = 0;
        for (int i = 0; i < BASELINE_SAMPLES; i++)
        {
            uint16_t sample = adc_read();
            sum += sample;
            if (sample < min_v)
                min_v = sample;
            if (sample > max_v)
                max_v = sample;
            sleep_us(200);
        }

        baseline = (uint16_t)(sum / BASELINE_SAMPLES);
        noise_floor = (uint16_t)(max_v - min_v);
        consecutive_hits = 0;
        initialized = true;
        return false;
    }

    baseline = (uint16_t)((baseline * 31 + v) / 32);

    int diff = (int)v - (int)baseline;
    if (diff < 0)

        diff = -diff;

    noise_floor = (uint16_t)((noise_floor * 31 + (uint16_t)diff) / 32);

    int dynamic_threshold = (int)noise_floor * NOISE_MULTIPLIER + NOISE_MARGIN;
    if (dynamic_threshold < SOUND_THRESHOLD_MIN)
        dynamic_threshold = SOUND_THRESHOLD_MIN;

    if (diff >= dynamic_threshold)
    {
        consecutive_hits++;
        if (consecutive_hits < TRIGGER_CONSECUTIVE_SAMPLES)
            return false;
    }

    {
        if (absolute_time_diff_us(last_trigger, get_absolute_time()) < COOLDOWN_US)

            return false;

        last_trigger = get_absolute_time();
        return true;
    }

    return false;
}