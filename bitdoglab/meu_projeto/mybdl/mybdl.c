#include <stdio.h>           // Biblioteca padrão da linguagem C
#include "pico/stdlib.h"     // Biblioteca fundamental do SDK do Pico
#include "pico/cyw43_arch.h" // Biblioteca que controla o chip Wi-Fi CYW43439 presente na Pico W

#define I2C_0_SDA 0 // Livre para periférico(s) na I2C_0
#define I2C_0_SCL 1 // Livre para periférico(s) na I2C_0

#define I2C_1_SDA 2 // Livre para periférico(s) na I2C_1
#define I2C_1_SCL 3 // Livre para periférico(s) na I2C_1

#define MICROFONE 28

#define OLED_SCL 15
#define OLED_SDA 14

#define JOYSTICK_X 26
#define JOYSTICK_Y 27
#define JOYSTICK_BT 22

#define MATRIZ_DE_LEDS 7 // Neopixel WS2018B

#define LED_RGB_R 13
#define LED_RGB_G 11
#define LED_RGB_B 12

#define BUZZER1 21
#define BUZZER2 10

#define BOTAO_A 5
#define BOTAO_B 6

int main()
{
    stdio_init_all(); // Essa função inicializa todos os canais de saída padrão disponíveis, como USB e UART, permitindo que o programa use printf para enviar mensagens para o console.
    sleep_ms(2000);   // Isso dá tempo para o PC detectar a porta USB e abrir o console antes que o programa comece a enviar mensagens.

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) // Essa função inicializa o chip CYW43439. Ela retorna 0 em caso de sucesso e um valor diferente de zero em caso de falha. Se a inicialização falhar, o programa imprime uma mensagem de erro e retorna -1 para indicar que ocorreu um erro.
    {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    // Example to turn on the Pico W LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); // Essa linha liga o LED da placa Pico W. A função cyw43_arch_gpio_put é usada para controlar os pinos GPIO do chip Wi-Fi. O primeiro argumento é o número do pino (CYW43_WL_GPIO_LED_PIN é uma constante que representa o pino do LED), e o segundo argumento é o valor a ser definido (1 para ligar o LED, 0 para desligar).

    while (true) // Esse é um loop infinito que mantém o programa em execução.
    {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
