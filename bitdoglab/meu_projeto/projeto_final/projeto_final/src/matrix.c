#include "matrix.h"
#include "config.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "ws2812.pio.h"

#define MATRIX_W 5
#define MATRIX_ROWS 5
#define MATRIX_N (MATRIX_W * MATRIX_ROWS)
#define MATRIX_RED_LEVEL 40
#define MATRIX_GREEN_LEVEL 20
#define MATRIX_BLUE_LEVEL 20

static PIO g_pio = pio0;
static uint g_sm = 0;

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

static rgb_t leds[MATRIX_N];

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
}

static void matrix_show(void)
{
    for (int i = 0; i < MATRIX_N; i++)
    {
        pio_sm_put_blocking(g_pio, g_sm, urgb_u32(leds[i].r, leds[i].g, leds[i].b) << 8u);
    }
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
    uint offset = pio_add_program(g_pio, &ws2812_program);
    ws2812_program_init(g_pio, g_sm, offset, MATRIX_PIN, 800000, false);

    matrix_clear();
    matrix_show();
}

void matrix_set_pattern(matrix_pattern_t pattern)
{
    matrix_clear();

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