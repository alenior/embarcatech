#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"

#define BUTTON_A_PIN 5
#define LED_PIN 12
#define INTERVALO_MS 50

volatile int contador = 0;
volatile bool blinking = false;
volatile int tempo_total = 0;

bool timer_callback(struct repeating_timer *t)
{
    if (blinking)
    {
        // Toggle direto (simples e eficiente)
        gpio_xor_mask(1 << LED_PIN);

        tempo_total += INTERVALO_MS;
        if(tempo_total % 1000 == 0) // A cada segundo
        {
            printf("Tempo total: %d segundo(s)\n", tempo_total / 1000);
        }

        // 10 segundos = 10000 ms
        if (tempo_total >= 10000)
        {
            blinking = false;
            tempo_total = 0;
            contador = 0;

            gpio_put(LED_PIN, 0); // garante desligado
            printf("Fim do piscar\n");
        }
    }

    return true;
}

int main()
{
    stdio_init_all();

    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    struct repeating_timer timer;
    add_repeating_timer_ms(-INTERVALO_MS, timer_callback, NULL, &timer);

    bool estado_anterior = 1;

    while (true)
    {
        bool estado_atual = gpio_get(BUTTON_A_PIN);

        // Detecta clique (borda)
        if (estado_anterior == 1 && estado_atual == 0 && !blinking)
        {
            contador++;
            printf("Contador: %d\n", contador);
            sleep_ms(50); // debounce simples

            if (contador >= 5)
            {
                blinking = true;
                tempo_total = 0;
                printf("Iniciando piscar...\n");
            }
        }

        estado_anterior = estado_atual;
    }
}