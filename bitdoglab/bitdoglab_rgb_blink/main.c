#include "pico/stdlib.h"

// Ajuste estes pinos conforme o hardware da BitDogLab
#define LED_R 13
#define LED_G 11
#define LED_B 12

int main()
{
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);

    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);

    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);

    while (true)
    {
        // verde ligado
        gpio_put(LED_G, 1);
        sleep_ms(500);

        // verde desligado
        gpio_put(LED_G, 0);
        sleep_ms(500);
    }
}