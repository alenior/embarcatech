/**
 * ============================================================
 * Driver SSD1306 — Display OLED 128×64 via I2C
 * BitDogLab v6.3  (I2C1: SDA=GPIO14, SCL=GPIO15, addr=0x3C)
 * ============================================================
 */

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

/* ── Dimensões ───────────────────────────────────────────── */
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64
#define SSD1306_PAGES (SSD1306_HEIGHT / 8) /* 8 páginas */

/* ── I2C (BitDogLab v6.3) ────────────────────────────────── */
#define SSD1306_I2C_PORT i2c1
#define SSD1306_I2C_SDA 14
#define SSD1306_I2C_SCL 15
#define SSD1306_I2C_ADDR 0x3C
#define SSD1306_I2C_FREQ 400000 /* 400 kHz fast-mode */

/* ── Cores lógicas ───────────────────────────────────────── */
#define SSD1306_BLACK 0
#define SSD1306_WHITE 1

/* ── Tamanhos de fonte ───────────────────────────────────── */
#define FONT_SMALL 1  /* 6×8  px por caractere (escala 1) */
#define FONT_MEDIUM 2 /* 12×16 px por caractere (escala 2) */
#define FONT_LARGE 3  /* 18×24 px por caractere (escala 3) */

/* ────────────────────────────────────────────────────────── */
/*  API pública                                               */
/* ────────────────────────────────────────────────────────── */

/** Inicializa I2C e o controlador SSD1306. */
void ssd1306_init(void);

/** Apaga o framebuffer (não envia ao display). */
void ssd1306_clear(void);

/** Envia o framebuffer inteiro ao display via I2C. */
void ssd1306_show(void);

/** Apaga buffer e atualiza o display em uma única chamada. */
void ssd1306_clear_show(void);

/** Acende ou apaga um pixel no framebuffer. */
void ssd1306_draw_pixel(int x, int y, uint8_t color);

/** Desenha linha horizontal. */
void ssd1306_draw_hline(int x, int y, int w, uint8_t color);

/** Desenha linha vertical. */
void ssd1306_draw_vline(int x, int y, int h, uint8_t color);

/** Desenha retângulo (contorno). */
void ssd1306_draw_rect(int x, int y, int w, int h, uint8_t color);

/** Desenha retângulo preenchido. */
void ssd1306_fill_rect(int x, int y, int w, int h, uint8_t color);

/**
 * Desenha um caractere ASCII (fonte 5×7 interna) na posição (x, y).
 * @param scale  1=normal, 2=dobro, 3=triplo
 */
void ssd1306_draw_char(int x, int y, char c, uint8_t color, uint8_t scale);

/**
 * Desenha string com fonte 5×7 interna.
 * @param scale  1, 2 ou 3
 * Retorna a coordenada X após o último caractere.
 */
int ssd1306_draw_string(int x, int y, const char *str,
                        uint8_t color, uint8_t scale);

/**
 * Barra de progresso horizontal.
 * @param x,y     canto superior-esquerdo
 * @param w,h     dimensões totais (incluindo borda)
 * @param percent 0–100
 */
void ssd1306_draw_progress_bar(int x, int y, int w, int h, int percent);

/**
 * Desenha ícone de onda sonora (3 arcos), útil como indicador visual.
 * @param cx,cy  centro
 * @param active true = branco, false = apagado
 */
void ssd1306_draw_sound_icon(int cx, int cy, bool active);

#endif /* SSD1306_H */