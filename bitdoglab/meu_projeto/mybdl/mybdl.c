#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include <string.h>

#include "ssd1306.h"

#define I2C_0_SDA 0
#define I2C_0_SCL 1

#define I2C_1_SDA 2
#define I2C_1_SCL 3

#define MICROFONE 28

#define OLED_SCL 15
#define OLED_SDA 14

#define JOYSTICK_X 26
#define JOYSTICK_Y 27
#define JOYSTICK_BT 22

#define MATRIZ_DE_LEDS 7

#define LED_RGB_R 13
#define LED_RGB_G 11
#define LED_RGB_B 12

#define BUZZER1 21
#define BUZZER2 10

#define BOTAO_A 5
#define BOTAO_B 6

#define WIFI_TIMEOUT_MS 30000

char ssid[32];
char password[64];

void oled_status(char *l1, char *l2, char *l3)
{
    ssd1306_clear();
    ssd1306_draw_string(0, 0, l1);
    ssd1306_draw_string(0, 16, l2);
    ssd1306_draw_string(0, 32, l3);
    ssd1306_show();
}

int main()
{
    stdio_init_all();
    // espera até 5 segundos pelo monitor serial
    for (int i = 0; i < 50; i++)
    {
        if (stdio_usb_connected())
            break;

        sleep_ms(100);
    }

    printf("USB conectado\n");
    printf("Sistema iniciando\n");

    // I2C para OLED
    i2c_init(i2c1, 400000);

    gpio_set_function(OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL, GPIO_FUNC_I2C);

    gpio_pull_up(OLED_SDA);
    gpio_pull_up(OLED_SCL);

    // Inicializa display
    ssd1306_init();
    ssd1306_clear();

    oled_status("Bem vindo", "Iniciando", "Informe wifi");
    ssd1306_update();
    sleep_ms(1000);

    printf("Inicializando WiFi...\n");
    sleep_ms(1000);

    if (cyw43_arch_init())
    {
        printf("WiFi init failed\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    oled_status("WiFi:", "Desconectado", "");
    ssd1306_update();
    sleep_ms(1000);

    printf("\n=== CONFIGURACAO WIFI ===\n");
    sleep_ms(500);

    oled_status("WiFi", "Digite SSID", "e senha");
    printf("Digite SSID \n");
    fflush(stdout);
    fgets(ssid, sizeof(ssid), stdin);
    ssid[strcspn(ssid, "\r\n")] = 0;
    printf("SSID recebido: %s\n", ssid);

    printf("Digite senha \n");
    fflush(stdout);
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\r\n")] = 0;
    printf("Senha recebida: %s\n", password);

    oled_status("WiFi:", "Conectando...", ssid);
    ssd1306_update();
    sleep_ms(1000);

    printf("Conectando em %s...\n", ssid);

    int result = cyw43_arch_wifi_connect_timeout_ms(
        ssid,
        password,
        CYW43_AUTH_WPA2_AES_PSK,
        WIFI_TIMEOUT_MS);

    if (result == 0)
    {
        oled_status("WiFi conectado", "Rede:", ssid);
        ssd1306_update();
        printf("Conectado!\n");
    }
    else
    {
        oled_status("WiFi", "Falha", "Não conectado");
        ssd1306_update();
        printf("Falha na conexao\n");
    }

    printf("Resultado da conexao: %d\n", result);

    while (true)
    {
        printf("Sistema ativo\n");
        sleep_ms(5000);
    }
}