#include "matrix.h"
#include "config.h"

#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/time.h"

#define MATRIX_W 5
#define MATRIX_ROWS 5
#define MATRIX_N (MATRIX_W * MATRIX_ROWS)
#define MATRIX_RED_LEVEL 40
#define MATRIX_GREEN_LEVEL 20
#define MATRIX_BLUE_LEVEL 20

typedef struct
{
    uint8_t g;
    uint8_t r;
    uint8_t b;
} grb_t;

static grb_t leds[MATRIX_N];

static inline void ws_send_bit(bool one)
{
    gpio_put(MATRIX_PIN, 1);
    if (one)
    {
        busy_wait_at_least_cycles(100); // ~0.8us @125MHz
        gpio_put(MATRIX_PIN, 0);
        busy_wait_at_least_cycles(56); // ~0.45us
    }
    else
    {
        busy_wait_at_least_cycles(50); // ~0.4us
        gpio_put(MATRIX_PIN, 0);
        busy_wait_at_least_cycles(106); // ~0.85us
    }
}

static void ws_send_byte(uint8_t v)
{
    for (int i = 7; i >= 0; i--)
        ws_send_bit((v >> i) & 1u);
}

static void matrix_show(void)
{
    uint32_t irq = save_and_disable_interrupts();
    for (int i = 0; i < MATRIX_N; i++)
    {
        ws_send_byte(leds[i].g);
        ws_send_byte(leds[i].r);
        ws_send_byte(leds[i].b);
    }
    restore_interrupts(irq);
    sleep_us(80);
}

static int map_xy(int x, int y)
{
    int row = y * MATRIX_W;
    if ((y & 1) == 0)
        return row + x;
    return row + (MATRIX_W - 1 - x);
}

static void matrix_clear(void)
{
    for (int i = 0; i < MATRIX_N; i++)
    {
        leds[i].r = 0;
        leds[i].g = 0;
        leds[i].b = 0;
    }
}

static void set_xy(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_ROWS)
        return;
    int i = map_xy(x, y);
    leds[i].r = r;
    leds[i].g = g;
    leds[i].b = b;
}

void matrix_init(void)
{
    gpio_init(MATRIX_PIN);
    gpio_set_dir(MATRIX_PIN, GPIO_OUT);
    gpio_put(MATRIX_PIN, 0);
    matrix_clear();
    matrix_show();
}

void matrix_set_pattern(matrix_pattern_t pattern)
{
    matrix_clear();
    matrix_show();

    if (pattern == MATRIX_PATTERN_TRIGGERED)
    {
        set_xy(2, 4, MATRIX_RED_LEVEL, 0, 0);
        set_xy(2, 3, MATRIX_RED_LEVEL, 0, 0);
        set_xy(2, 2, MATRIX_RED_LEVEL, 0, 0);
        set_xy(2, 0, MATRIX_RED_LEVEL, 0, 0);
    }
    else if (pattern == MATRIX_PATTERN_DISARMED)
    {
        for (int i = 0; i < MATRIX_W; i++)
        {
            set_xy(i, i, 0, MATRIX_GREEN_LEVEL, 0);
            set_xy(MATRIX_W - 1 - i, i, 0, MATRIX_GREEN_LEVEL, 0);
        }
    }
    else if (pattern == MATRIX_PATTERN_ARMED)
    {
        for (int x = 0; x < MATRIX_W; x++)
        {
            set_xy(x, 0, 0, 0, MATRIX_BLUE_LEVEL);
            set_xy(x, MATRIX_ROWS - 1, 0, 0, MATRIX_BLUE_LEVEL);
        }
        for (int y = 0; y < MATRIX_ROWS; y++)
        {
            set_xy(0, y, 0, 0, MATRIX_BLUE_LEVEL);
            set_xy(MATRIX_W - 1, y, 0, 0, MATRIX_BLUE_LEVEL);
        }
    }

    matrix_show();
}