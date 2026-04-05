#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/sync.h"
#include "hardware/i2c.h"
#include "pico/cyw43_arch.h"
#include "hardware/watchdog.h"

// ================= PINOS =================
#define PIR_PIN 8
#define AUDIO_PIN 28

#define BTN_A 5
#define BTN_B 6

#define JOY_X 26
#define JOY_Y 27

absolute_time_t start_time;

// ================= SENHA =================
#define PASSWORD_LENGTH 4

const char senha_correta[PASSWORD_LENGTH + 1] = "5555";
char senha_digitada[PASSWORD_LENGTH + 1];
int senha_index = 0;

int current_digit = 0;

// ================= AUDIO =================
uint16_t audio_baseline = 0;
bool audio_initialized = false;

absolute_time_t last_sound_trigger = 0;
#define SOUND_COOLDOWN_US 2000000 // 2 segundos

bool detect_sound()
{
    adc_select_input(2);

    uint16_t max_val = 0;
    uint16_t min_val = 4095;

    for (int i = 0; i < 16; i++)
    {
        uint16_t v = adc_read();

        if (v > max_val)
            max_val = v;
        if (v < min_val)
            min_val = v;

        sleep_us(20);
    }

    uint16_t amplitude = max_val - min_val;

    // Inicialização do baseline
    if (!audio_initialized)
    {
        audio_baseline = amplitude;
        audio_initialized = true;
        return false;
    }

    // Filtro mais estável (IIR)
    audio_baseline = (audio_baseline * 7 + amplitude) / 8;

    int diff = amplitude - audio_baseline;
    if (diff < 0)
        diff = -diff;

    bool detected = diff > 150; // ligeiramente mais sensível

    if (detected)
    {
        if (absolute_time_diff_us(last_sound_trigger, get_absolute_time()) < SOUND_COOLDOWN_US)
            return false;

        last_sound_trigger = get_absolute_time();

        printf("SOM DETECTADO | diff=%d\n", diff); // debug útil
        return true;
    }

    return false;
}

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
    adc_gpio_init(AUDIO_PIN);
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

        if (system_ready)
        {
            pir = get_pir_event();
            sound = detect_sound();
        }
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
            15000))
    {
        printf("WiFi conectado\n");
    }
}

// ================= DISPLAY =================

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define OLED_ADDR 0x3C

#define OLED_WIDTH 128
#define OLED_HEIGHT 64

uint8_t oled_buffer[OLED_WIDTH * OLED_HEIGHT / 8];

bool oled_ok = false;

void oled_command(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};

    int res = i2c_write_blocking(I2C_PORT, OLED_ADDR, buf, 2, false);

    if (res < 0)
    {
        printf("Erro I2C (cmd)\n");
        oled_ok = false;
    }
}

void oled_data(uint8_t *data, size_t len)
{
    uint8_t buf[17]; // máximo seguro por transmissão

    for (size_t i = 0; i < len; i += 16)
    {
        size_t chunk = (len - i > 16) ? 16 : (len - i);

        buf[0] = 0x40;
        memcpy(&buf[1], &data[i], chunk);

        int res = i2c_write_blocking(I2C_PORT, OLED_ADDR, buf, chunk + 1, false);

        if (res < 0)
        {
            printf("Erro I2C (data)\n");
            oled_ok = false;
            return; // interrompe envio
        }
    }
}

void oled_init()
{
    i2c_init(I2C_PORT, 100 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    sleep_ms(100);

    oled_command(0xAE); // OFF
    oled_command(0x20);
    oled_command(0x00);
    oled_command(0xB0);
    oled_command(0xC8);
    oled_command(0x00);
    oled_command(0x10);
    oled_command(0x40);
    oled_command(0x81);
    oled_command(0xFF);
    oled_command(0xA1);
    oled_command(0xA6);
    oled_command(0xA8);
    oled_command(0x3F);
    oled_command(0xA4);
    oled_command(0xD3);
    oled_command(0x00);
    oled_command(0xD5);
    oled_command(0xF0);
    oled_command(0xD9);
    oled_command(0x22);
    oled_command(0xDA);
    oled_command(0x12);
    oled_command(0xDB);
    oled_command(0x20);
    oled_command(0x8D);
    oled_command(0x14);
    oled_command(0xAF); // ON

    uint8_t dummy = 0x00;
    int res = i2c_write_blocking(I2C_PORT, OLED_ADDR, &dummy, 1, false);

    if (res >= 0)
    {
        oled_ok = true;
        printf("OLED detectado\n");
    }
    else
    {
        oled_ok = false;
        printf("OLED NAO encontrado\n");
        return;
    }
}

void oled_clear()
{
    memset(oled_buffer, 0, sizeof(oled_buffer));
}

void oled_update()
{
    if (!oled_ok)
        return;

    for (int page = 0; page < 8; page++)
    {
        oled_command(0xB0 + page);
        oled_command(0x00);
        oled_command(0x10);

        oled_data(&oled_buffer[OLED_WIDTH * page], OLED_WIDTH);
    }
}

const uint8_t font5x7[96][5] = {
    // ASCII 32–127

    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /

    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9

    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?

    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @

    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z

    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // '\'
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
};

void oled_draw_char(int x, int y, char c)
{
    if (c < 32 || c > 127)
        return;

    int index = c - 32;

    const uint8_t *bitmap = font5x7[index];

    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (bitmap[i] & (1 << j))
            {
                int px = x + i;
                int py = y + j;

                int index_buf = px + (py / 8) * OLED_WIDTH;
                oled_buffer[index_buf] |= (1 << (py % 8));
            }
        }
    }
}

void oled_draw_char_scaled(int x, int y, char c, int scale)
{
    // Faixa ASCII suportada (32 a 127)
    if (c < 32 || c > 127)
        return;

    int index = c - 32; // Mapeamento direto ASCII

    const uint8_t *bitmap = font5x7[index];

    for (int i = 0; i < 5; i++) // largura
    {
        for (int j = 0; j < 8; j++) // altura
        {
            if (bitmap[i] & (1 << j))
            {
                for (int dx = 0; dx < scale; dx++)
                {
                    for (int dy = 0; dy < scale; dy++)
                    {
                        int px = x + (i * scale) + dx;
                        int py = y + (j * scale) + dy;

                        // Proteção contra overflow de buffer
                        if (px < 0 || px >= OLED_WIDTH || py < 0 || py >= OLED_HEIGHT)
                            continue;

                        int index_buf = px + (py / 8) * OLED_WIDTH;
                        oled_buffer[index_buf] |= (1 << (py % 8));
                    }
                }
            }
        }
    }
}

void oled_draw_string(int x, int y, const char *str)
{
    while (*str)
    {
        oled_draw_char(x, y, *str);
        x += 6;
        str++;
    }
}

void oled_draw_string_scaled(int x, int y, const char *str, int scale)
{
    while (*str)
    {
        oled_draw_char_scaled(x, y, *str, scale);
        x += (6 * scale);
        str++;
    }
}

int oled_get_centered_x(const char *str, int scale)
{
    int len = strlen(str);
    int width = len * 6 * scale;
    return (OLED_WIDTH - width) / 2;
}

void oled_show_status()
{
    if (!oled_ok)
        return;

    oled_clear();

    // ================= STATUS (GRANDE E LIMPO) =================
    const char *status;

    if (state == ALARM_DISARMED)
        status = "DESARMADO";
    else if (state == ALARM_ARMED)
        status = "ARMADO";
    else
        status = "ALERTA";

    int x_center = oled_get_centered_x(status, 2);
    oled_draw_string_scaled(x_center, 0, status, 2);

    // ================= LINHA SEPARADORA =================
    for (int x = 0; x < OLED_WIDTH; x++)
    {
        int index = x + (24 / 8) * OLED_WIDTH;
        oled_buffer[index] |= (1 << (24 % 8));
    }

    // ================= DIGITO =================
    char buf[16];
    sprintf(buf, "ATUAL: %d", current_digit);
    oled_draw_string(0, 30, buf);

    // ================= SENHA =================
    oled_draw_string(0, 45, "SENHA: ");

    for (int i = 0; i < senha_index; i++)
    {
        oled_draw_char(35 + i * 6, 45, '*');
    }

    // Cursor piscando
    static bool blink = false;
    blink = !blink;

    if (blink && senha_index < PASSWORD_LENGTH)
    {
        oled_draw_char(35 + senha_index * 6, 45, '*');
    }

    oled_update();
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
    init_wifi();
    watchdog_enable(15000, 1); // 15 segundos
    init_leds();
    oled_init();

    state = ALARM_ARMED;

    while (true)
    {
        watchdog_update();

        cyw43_arch_poll();

        process_alarm();

        oled_show_status();

        for (int i = 0; i < 10; i++)
        {
            sleep_ms(10);
            cyw43_arch_poll();
        }
    }
}