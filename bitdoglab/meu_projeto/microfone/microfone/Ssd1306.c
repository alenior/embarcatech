/**
 * ============================================================
 * Driver SSD1306 — Display OLED 128×64 via I2C
 * Fonte 5×7 ASCII embutida, sem dependências externas.
 * ============================================================
 */

#include "ssd1306.h"
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <math.h>

/* ── Framebuffer (1 bit por pixel, organizado em páginas) ── */
static uint8_t s_fb[SSD1306_PAGES][SSD1306_WIDTH];

/* ────────────────────────────────────────────────────────── */
/*  Sequência de inicialização do controlador SSD1306         */
/* ────────────────────────────────────────────────────────── */
static const uint8_t k_init_cmds[] = {
    0xAE,       /* Display OFF                              */
    0xD5, 0x80, /* Clock divide ratio / oscillator freq     */
    0xA8, 0x3F, /* Multiplex ratio = 64                     */
    0xD3, 0x00, /* Display offset = 0                       */
    0x40,       /* Start line = 0                           */
    0x8D, 0x14, /* Charge pump habilitado (Vcc interno)     */
    0x20, 0x00, /* Memory addressing mode = horizontal      */
    0xA1,       /* Segment remap (col 127 → SEG0)           */
    0xC8,       /* COM scan direction: decrescente          */
    0xDA, 0x12, /* COM pins hardware config                 */
    0x81, 0xCF, /* Contrast = 207                           */
    0xD9, 0xF1, /* Pre-charge period                        */
    0xDB, 0x40, /* VCOMH deselect level                     */
    0xA4,       /* Exibe conteúdo da RAM (não forçado)      */
    0xA6,       /* Normal display (não invertido)           */
    0x2E,       /* Scroll desativado                        */
    0xAF        /* Display ON                               */
};

/* ────────────────────────────────────────────────────────── */
/*  Fonte 5×7 ASCII (caracteres 0x20–0x7E)                   */
/*  Cada entrada: 5 bytes de coluna, do topo (bit0) ao fundo */
/* ────────────────────────────────────────────────────────── */
static const uint8_t k_font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /* 0x20  ESPAÇO */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* 0x21  !      */
    {0x00, 0x07, 0x00, 0x07, 0x00}, /* 0x22  "      */
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, /* 0x23  #      */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /* 0x24  $      */
    {0x23, 0x13, 0x08, 0x64, 0x62}, /* 0x25  %      */
    {0x36, 0x49, 0x55, 0x22, 0x50}, /* 0x26  &      */
    {0x00, 0x05, 0x03, 0x00, 0x00}, /* 0x27  '      */
    {0x00, 0x1C, 0x22, 0x41, 0x00}, /* 0x28  (      */
    {0x00, 0x41, 0x22, 0x1C, 0x00}, /* 0x29  )      */
    {0x14, 0x08, 0x3E, 0x08, 0x14}, /* 0x2A  *      */
    {0x08, 0x08, 0x3E, 0x08, 0x08}, /* 0x2B  +      */
    {0x00, 0x50, 0x30, 0x00, 0x00}, /* 0x2C  ,      */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* 0x2D  -      */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /* 0x2E  .      */
    {0x20, 0x10, 0x08, 0x04, 0x02}, /* 0x2F  /      */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0x30  0      */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 0x31  1      */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 0x32  2      */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 0x33  3      */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 0x34  4      */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 0x35  5      */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 0x36  6      */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 0x37  7      */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 0x38  8      */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 0x39  9      */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* 0x3A  :      */
    {0x00, 0x56, 0x36, 0x00, 0x00}, /* 0x3B  ;      */
    {0x08, 0x14, 0x22, 0x41, 0x00}, /* 0x3C  <      */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /* 0x3D  =      */
    {0x00, 0x41, 0x22, 0x14, 0x08}, /* 0x3E  >      */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* 0x3F  ?      */
    {0x32, 0x49, 0x79, 0x41, 0x3E}, /* 0x40  @      */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* 0x41  A      */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* 0x42  B      */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* 0x43  C      */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* 0x44  D      */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* 0x45  E      */
    {0x7F, 0x09, 0x09, 0x09, 0x01}, /* 0x46  F      */
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, /* 0x47  G      */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* 0x48  H      */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* 0x49  I      */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* 0x4A  J      */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* 0x4B  K      */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* 0x4C  L      */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, /* 0x4D  M      */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* 0x4E  N      */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* 0x4F  O      */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* 0x50  P      */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* 0x51  Q      */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* 0x52  R      */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* 0x53  S      */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* 0x54  T      */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* 0x55  U      */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* 0x56  V      */
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, /* 0x57  W      */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* 0x58  X      */
    {0x07, 0x08, 0x70, 0x08, 0x07}, /* 0x59  Y      */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* 0x5A  Z      */
    {0x00, 0x7F, 0x41, 0x41, 0x00}, /* 0x5B  [      */
    {0x02, 0x04, 0x08, 0x10, 0x20}, /* 0x5C  \      */
    {0x00, 0x41, 0x41, 0x7F, 0x00}, /* 0x5D  ]      */
    {0x04, 0x02, 0x01, 0x02, 0x04}, /* 0x5E  ^      */
    {0x40, 0x40, 0x40, 0x40, 0x40}, /* 0x5F  _      */
    {0x00, 0x01, 0x02, 0x04, 0x00}, /* 0x60  `      */
    {0x20, 0x54, 0x54, 0x54, 0x78}, /* 0x61  a      */
    {0x7F, 0x48, 0x44, 0x44, 0x38}, /* 0x62  b      */
    {0x38, 0x44, 0x44, 0x44, 0x20}, /* 0x63  c      */
    {0x38, 0x44, 0x44, 0x48, 0x7F}, /* 0x64  d      */
    {0x38, 0x54, 0x54, 0x54, 0x18}, /* 0x65  e      */
    {0x08, 0x7E, 0x09, 0x01, 0x02}, /* 0x66  f      */
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, /* 0x67  g      */
    {0x7F, 0x08, 0x04, 0x04, 0x78}, /* 0x68  h      */
    {0x00, 0x44, 0x7D, 0x40, 0x00}, /* 0x69  i      */
    {0x20, 0x40, 0x44, 0x3D, 0x00}, /* 0x6A  j      */
    {0x7F, 0x10, 0x28, 0x44, 0x00}, /* 0x6B  k      */
    {0x00, 0x41, 0x7F, 0x40, 0x00}, /* 0x6C  l      */
    {0x7C, 0x04, 0x18, 0x04, 0x78}, /* 0x6D  m      */
    {0x7C, 0x08, 0x04, 0x04, 0x78}, /* 0x6E  n      */
    {0x38, 0x44, 0x44, 0x44, 0x38}, /* 0x6F  o      */
    {0x7C, 0x14, 0x14, 0x14, 0x08}, /* 0x70  p      */
    {0x08, 0x14, 0x14, 0x18, 0x7C}, /* 0x71  q      */
    {0x7C, 0x08, 0x04, 0x04, 0x08}, /* 0x72  r      */
    {0x48, 0x54, 0x54, 0x54, 0x20}, /* 0x73  s      */
    {0x04, 0x3F, 0x44, 0x40, 0x20}, /* 0x74  t      */
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, /* 0x75  u      */
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, /* 0x76  v      */
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, /* 0x77  w      */
    {0x44, 0x28, 0x10, 0x28, 0x44}, /* 0x78  x      */
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, /* 0x79  y      */
    {0x44, 0x64, 0x54, 0x4C, 0x44}, /* 0x7A  z      */
    {0x00, 0x08, 0x36, 0x41, 0x00}, /* 0x7B  {      */
    {0x00, 0x00, 0x7F, 0x00, 0x00}, /* 0x7C  |      */
    {0x00, 0x41, 0x36, 0x08, 0x00}, /* 0x7D  }      */
    {0x10, 0x08, 0x08, 0x10, 0x08}, /* 0x7E  ~      */
};

/* ────────────────────────────────────────────────────────── */
/*  Helpers I2C                                               */
/* ────────────────────────────────────────────────────────── */

static void i2c_send_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    i2c_write_blocking(SSD1306_I2C_PORT, SSD1306_I2C_ADDR, buf, 2, false);
}

static void i2c_send_data(const uint8_t *data, size_t len)
{
    /* Prefixo 0x40 = modo dado */
    uint8_t buf[SSD1306_WIDTH + 1];
    buf[0] = 0x40;
    memcpy(buf + 1, data, len);
    i2c_write_blocking(SSD1306_I2C_PORT, SSD1306_I2C_ADDR,
                       buf, len + 1, false);
}

/* ────────────────────────────────────────────────────────── */
/*  Inicialização                                             */
/* ────────────────────────────────────────────────────────── */

void ssd1306_init(void)
{
    /* Configura I2C */
    i2c_init(SSD1306_I2C_PORT, SSD1306_I2C_FREQ);
    gpio_set_function(SSD1306_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(SSD1306_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(SSD1306_I2C_SDA);
    gpio_pull_up(SSD1306_I2C_SCL);

    sleep_ms(10); /* aguarda o display estabilizar após power-on */

    /* Envia sequência de inicialização */
    for (size_t i = 0; i < sizeof(k_init_cmds); i++)
    {
        i2c_send_cmd(k_init_cmds[i]);
    }

    ssd1306_clear();
    ssd1306_show();
}

/* ────────────────────────────────────────────────────────── */
/*  Framebuffer                                               */
/* ────────────────────────────────────────────────────────── */

void ssd1306_clear(void)
{
    memset(s_fb, 0, sizeof(s_fb));
}

void ssd1306_show(void)
{
    /* Configura janela de endereçamento para o display todo */
    i2c_send_cmd(0x21);
    i2c_send_cmd(0);
    i2c_send_cmd(127); /* colunas 0–127 */
    i2c_send_cmd(0x22);
    i2c_send_cmd(0);
    i2c_send_cmd(7); /* páginas 0–7   */

    /* Envia cada página em um único write I2C */
    for (int p = 0; p < SSD1306_PAGES; p++)
    {
        i2c_send_data(s_fb[p], SSD1306_WIDTH);
    }
}

void ssd1306_clear_show(void)
{
    ssd1306_clear();
    ssd1306_show();
}

/* ────────────────────────────────────────────────────────── */
/*  Primitivas de desenho                                     */
/* ────────────────────────────────────────────────────────── */

void ssd1306_draw_pixel(int x, int y, uint8_t color)
{
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT)
        return;
    int page = y / 8;
    int bit = y % 8;
    if (color)
        s_fb[page][x] |= (1 << bit);
    else
        s_fb[page][x] &= ~(1 << bit);
}

void ssd1306_draw_hline(int x, int y, int w, uint8_t color)
{
    for (int i = 0; i < w; i++)
        ssd1306_draw_pixel(x + i, y, color);
}

void ssd1306_draw_vline(int x, int y, int h, uint8_t color)
{
    for (int i = 0; i < h; i++)
        ssd1306_draw_pixel(x, y + i, color);
}

void ssd1306_draw_rect(int x, int y, int w, int h, uint8_t color)
{
    ssd1306_draw_hline(x, y, w, color);
    ssd1306_draw_hline(x, y + h - 1, w, color);
    ssd1306_draw_vline(x, y, h, color);
    ssd1306_draw_vline(x + w - 1, y, h, color);
}

void ssd1306_fill_rect(int x, int y, int w, int h, uint8_t color)
{
    for (int row = 0; row < h; row++)
        ssd1306_draw_hline(x, y + row, w, color);
}

/* ────────────────────────────────────────────────────────── */
/*  Texto (fonte 5×7 com escala)                              */
/* ────────────────────────────────────────────────────────── */

void ssd1306_draw_char(int x, int y, char c, uint8_t color, uint8_t scale)
{
    if (c < 0x20 || c > 0x7E)
        c = '?';
    const uint8_t *glyph = k_font5x7[c - 0x20];

    for (int col = 0; col < 5; col++)
    {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++)
        {
            if (line & (1 << row))
            {
                /* Preenche scale×scale pixels por "pixel lógico" */
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        ssd1306_draw_pixel(x + col * scale + sx,
                                           y + row * scale + sy,
                                           color);
            }
        }
    }
}

int ssd1306_draw_string(int x, int y, const char *str,
                        uint8_t color, uint8_t scale)
{
    int cx = x;
    while (*str)
    {
        ssd1306_draw_char(cx, y, *str++, color, scale);
        cx += (5 + 1) * scale; /* 5 cols + 1 de espaçamento */
    }
    return cx;
}

/* ────────────────────────────────────────────────────────── */
/*  Barra de progresso                                        */
/* ────────────────────────────────────────────────────────── */

void ssd1306_draw_progress_bar(int x, int y, int w, int h, int percent)
{
    if (percent < 0)
        percent = 0;
    if (percent > 100)
        percent = 100;

    /* Borda externa */
    ssd1306_draw_rect(x, y, w, h, SSD1306_WHITE);

    /* Preenchimento interno proporcional */
    int inner_w = ((w - 4) * percent) / 100;
    if (inner_w > 0)
        ssd1306_fill_rect(x + 2, y + 2, inner_w, h - 4, SSD1306_WHITE);
}

/* ────────────────────────────────────────────────────────── */
/*  Ícone de onda sonora (3 arcos concêntricos simplificados) */
/* ────────────────────────────────────────────────────────── */

void ssd1306_draw_sound_icon(int cx, int cy, bool active)
{
    uint8_t color = active ? SSD1306_WHITE : SSD1306_BLACK;

    /* Ponto central (speaker) */
    ssd1306_fill_rect(cx - 2, cy - 2, 5, 5, color);

    if (!active)
        return;

    /* Arco 1 (pequeno) — 3 px de raio */
    for (int dy = -3; dy <= 3; dy++)
    {
        int dx = (int)sqrt(9 - dy * dy);
        ssd1306_draw_pixel(cx + dx, cy + dy, color);
    }
    /* Arco 2 (médio) — 5 px de raio */
    for (int dy = -5; dy <= 5; dy++)
    {
        int dx = (int)sqrt(25 - dy * dy);
        ssd1306_draw_pixel(cx + dx, cy + dy, color);
    }
    /* Arco 3 (grande) — 7 px de raio */
    for (int dy = -7; dy <= 7; dy++)
    {
        int dx = (int)sqrt(49 - dy * dy);
        ssd1306_draw_pixel(cx + dx, cy + dy, color);
    }
}