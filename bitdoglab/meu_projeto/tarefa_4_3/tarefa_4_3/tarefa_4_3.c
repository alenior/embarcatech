#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

#define LED_RED 13
#define LED_BLUE 12

#define WRAP_RED 999

// Variável global de duty cycle (%)
volatile uint8_t duty_red = 5;

// Slice do PWM
uint slice_red;

// Callback do timer (executa a cada 2 segundos)
bool timer_callback(struct repeating_timer *t)
{
    duty_red += 5;

    if (duty_red > 100)
        duty_red = 5;

    // Atualiza duty cycle
    uint16_t level = (WRAP_RED * duty_red) / 100;

    pwm_set_gpio_level(LED_RED, level);

    printf("Duty RED: %d%%\n", duty_red);

    return true; // continua repetindo
}

int main()
{
    stdio_init_all();

    // ==============================
    // 🔴 CONFIG PWM LED VERMELHO (1 kHz)
    // ==============================
    gpio_set_function(LED_RED, GPIO_FUNC_PWM);
    slice_red = pwm_gpio_to_slice_num(LED_RED);

    pwm_set_clkdiv(slice_red, 125.0f); // divisor
    pwm_set_wrap(slice_red, WRAP_RED); // resolução (~1000 passos)

    uint16_t level_red = (WRAP_RED * duty_red) / 100;
    pwm_set_gpio_level(LED_RED, level_red);

    pwm_set_enabled(slice_red, true);

    // ==============================
    // 🔵 CONFIG PWM LED AZUL (10 kHz)
    // ==============================
    gpio_set_function(LED_BLUE, GPIO_FUNC_PWM);
    uint slice_blue = pwm_gpio_to_slice_num(LED_BLUE);

    pwm_set_clkdiv(slice_blue, 125.0f);
    pwm_set_wrap(slice_blue, 99); // 10kHz

    // Duty fixo 50%
    pwm_set_gpio_level(LED_BLUE, 50);

    pwm_set_enabled(slice_blue, true);

    // ==============================
    // ⏱️ TIMER (interrupção a cada 2s)
    // ==============================
    struct repeating_timer timer;
    add_repeating_timer_ms(2000, timer_callback, NULL, &timer);

    // Loop principal
    while (true)
    {
        tight_loop_contents();
    }
}