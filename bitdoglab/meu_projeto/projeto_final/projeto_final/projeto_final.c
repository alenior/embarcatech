#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "pico/cyw43_arch.h"

// ================= PINOS =================
#define PIR_PIN 8
#define AUDIO_PIN 28

#define BTN_A 5
#define BTN_B 6

#define JOY_X 26
#define JOY_Y 27

absolute_time_t start_time;

// ================= SENHA =================
#define PASSWORD_LENGTH 8

const char senha_correta[PASSWORD_LENGTH + 1] = "12345678";
char senha_digitada[PASSWORD_LENGTH + 1];
int senha_index = 0;

int current_digit = 0;

// ================= AUDIO =================
#define AUDIO_BUFFER_SIZE 256
uint16_t audio_buffer[AUDIO_BUFFER_SIZE];
int dma_chan;

// ================= ESTADOS =================
typedef enum
{
    ALARM_DISARMED,
    ALARM_ARMED,
    ALARM_TRIGGERED
} alarm_state_t;

alarm_state_t state = ALARM_DISARMED;
alarm_state_t last_state = -1;

// ================= PIR =================
volatile bool pir_triggered = false;

void pir_callback(uint gpio, uint32_t events)
{
    pir_triggered = true;
}

void init_pir()
{
    gpio_init(PIR_PIN);
    gpio_set_dir(PIR_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(
        PIR_PIN,
        GPIO_IRQ_EDGE_RISE,
        true,
        &pir_callback);
}

// ================= BOTÕES =================
void init_buttons()
{
    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);

    gpio_init(BTN_B);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_B);
}

bool button_pressed(uint pin)
{
    static absolute_time_t last_press[30] = {0};

    if (gpio_get(pin) == 0)
    {
        if (absolute_time_diff_us(last_press[pin], get_absolute_time()) > 200000)
        {
            last_press[pin] = get_absolute_time();
            return true;
        }
    }
    return false;
}

// ================= ADC JOYSTICK =================
void init_joystick()
{
    adc_init();
    adc_gpio_init(JOY_X);
    adc_gpio_init(JOY_Y);
}

// ================= AUDIO =================
void init_audio_dma()
{
    adc_init();
    adc_gpio_init(AUDIO_PIN);
    adc_select_input(2);

    adc_fifo_setup(true, true, 1, false, false);

    dma_chan = dma_claim_unused_channel(true);

    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    dma_channel_configure(
        dma_chan,
        &cfg,
        audio_buffer,
        &adc_hw->fifo,
        AUDIO_BUFFER_SIZE,
        true);
}

bool detect_sound()
{
    adc_select_input(2);

    uint32_t sum = 0;

    for (int i = 0; i < 100; i++)
    {
        sum += adc_read();
        sleep_us(50);
    }

    return (sum / 100) > 2500;
}

// ================= INPUT =================
int read_joystick_digit()
{
    adc_select_input(0);
    uint16_t x = adc_read();

    if (x < 1000)
        return (current_digit + 9) % 10;
    else if (x > 3000)
        return (current_digit + 1) % 10;

    return current_digit;
}

// ================= SENHA =================
void reset_password()
{
    senha_index = 0;
    memset(senha_digitada, 0, sizeof(senha_digitada));
}

bool password_ready = false;

void handle_password_input()
{
    current_digit = read_joystick_digit();
    static int last_index = -1;
    static int last_digit = -1;

    if (senha_index != last_index || current_digit != last_digit)
    {
        last_index = senha_index;
        last_digit = current_digit;

        printf("Atual: %d | Senha: ", current_digit);
        for (int i = 0; i < senha_index; i++)
            printf("*");
        printf("\n");
    }

    if (button_pressed(BTN_A))
    {
        sleep_ms(200);

        if (senha_index < PASSWORD_LENGTH)
        {
            senha_digitada[senha_index++] = '0' + current_digit;
            printf("Digitado: %d\n", current_digit);
        }
    }

    if (button_pressed(BTN_B))
    {
        sleep_ms(200);

        if (senha_index > 0)
        {
            senha_index--;
            printf("Apagado\n");
        }
    }

    if (senha_index == PASSWORD_LENGTH)
    {
        senha_digitada[senha_index] = '\0';
        password_ready = true;
    }
}

bool check_password()
{
    if (!password_ready)
        return false;

    password_ready = false;

    return strcmp(senha_digitada, senha_correta) == 0;
}

// ================= LEDS =================

#define LED_R 13
#define LED_G 11
#define LED_B 12

void init_leds()
{
    gpio_init(LED_R);
    gpio_set_dir(LED_R, GPIO_OUT);

    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);

    gpio_init(LED_B);
    gpio_set_dir(LED_B, GPIO_OUT);
}

void set_led(bool r, bool g, bool b)
{
    gpio_put(LED_R, r);
    gpio_put(LED_G, g);
    gpio_put(LED_B, b);
}

// ================= ALARME =================
bool get_pir_event()
{
    bool val;
    uint32_t irq = save_and_disable_interrupts();
    val = pir_triggered;
    pir_triggered = false;
    restore_interrupts(irq);
    return val;
}

void process_alarm()
{
    bool pir = false;
    bool sound = false;

    if (state == ALARM_ARMED)
    {
        bool system_ready = absolute_time_diff_us(start_time, get_absolute_time()) > 5000000;

        if (system_ready && (pir || sound))
        {
            state = ALARM_TRIGGERED;
        }

        // temporariamente desabilitado
        // sound = detect_sound();
    }

    if (state != last_state)
    {
        last_state = state;

        switch (state)
        {
        case ALARM_DISARMED:
            printf("DESARMADO - digite senha para ARMAR\n");
            break;

        case ALARM_ARMED:
            printf("SISTEMA ARMADO\n");
            break;

        case ALARM_TRIGGERED:
            printf("ALARME DISPARADO! DIGITE SENHA\n");
            break;
        }
    }

    switch (state)
    {
    case ALARM_DISARMED:
        set_led(0, 0, 1);

        handle_password_input();

        if (check_password())
        {
            state = ALARM_ARMED;
            reset_password();
        }
        break;

    case ALARM_ARMED:
        set_led(0, 1, 0);

        if (pir || sound)
        {
            state = ALARM_TRIGGERED;
        }

        handle_password_input();

        if (check_password())
        {
            state = ALARM_DISARMED;
            reset_password();
        }
        break;

    case ALARM_TRIGGERED:
        set_led(1, 0, 0);

        handle_password_input();

        if (check_password())
        {
            state = ALARM_DISARMED;
            reset_password();
        }
        break;
    }
}

// ================= WIFI =================
void init_wifi()
{
    if (cyw43_arch_init())
    {
        printf("Erro WiFi\n");
        return;
    }

    cyw43_arch_enable_sta_mode();

    if (!cyw43_arch_wifi_connect_timeout_ms(
            "Marias",
            "88526352",
            CYW43_AUTH_WPA2_AES_PSK,
            30000))
    {
        printf("WiFi conectado\n");
    }
}

// ================= MAIN =================
int main()
{
    stdio_init_all();
    sleep_ms(2000);

    printf("BOOT OK\n");

    start_time = get_absolute_time();

    init_pir();
    init_buttons();
    init_joystick();
    init_audio_dma();
    init_wifi();
    init_leds();

    state = ALARM_ARMED;

    while (true)
    {
        cyw43_arch_poll();

        process_alarm();

        sleep_ms(100);
    }
}