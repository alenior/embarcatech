/**
 * ============================================================
 * Detector de Presença por Som Ambiente — com Display OLED
 * BitDogLab v6.3 — Raspberry Pi Pico W
 * ============================================================
 *
 * Layout do display OLED SSD1306 128×64:
 *
 *  [MODO IDLE]
 *  ┌────────────────────────────────┐
 *  │ ))) DET. SONORO       #00012   │  y=0   cabeçalho
 *  ├────────────────────────────────┤
 *  │ RMS  [█████████░░░░░░░]  142   │  y=14  nível + barra
 *  ├────────────────────────────────┤
 *  │ AGUARDANDO...                  │  y=28  status
 *  │ Ult:00008.423s R: 214          │  y=38  último evento
 *  ├────────────────────────────────┤
 *  │ IP:192.168.1.42                │  y=52  Wi-Fi
 *  └────────────────────────────────┘
 *
 *  [MODO ALERTA]
 *  ╔════════════════════════════════╗
 *  ║  !! EVENTO DETECTADO !!        ║  y=3   cabeçalho invertido
 *  ╠════════════════════════════════╣
 *  ║  RMS: 287.3         #00013     ║  y=18  valor em destaque
 *  ║  [████████████████████████]    ║  y=36  barra cheia
 *  ║  T: 00012.543 s                ║  y=47  timestamp
 *  ╠════════════════════════════════╣
 *  ║  HTTP: notificando...          ║  y=57  status HTTP
 *  ╚════════════════════════════════╝
 *
 * Hardware (BitDogLab v6.3):
 *   Microfone ADC  : GPIO 28 (ADC canal 2)
 *   OLED SSD1306   : I2C1, SDA=GPIO14, SCL=GPIO15, addr=0x3C
 *   LED vermelho   : GPIO 13 (alerta de evento)
 *   LED azul       : GPIO 12 (Wi-Fi conectando)
 *   LED verde      : GPIO 11 (idle / standby)
 *
 * Dependências (pico-sdk):
 *   pico_stdlib, pico_cyw43_arch_lwip_poll,
 *   hardware_adc, hardware_dma, hardware_timer, hardware_i2c, m
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/timer.h"

#include "lwip/tcp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

#include "ssd1306.h"

/* ============================================================
 * Configurações de Hardware
 * ============================================================ */
#define MIC_ADC_GPIO 28
#define MIC_ADC_CHANNEL 2
/**
 * Viés DC do microfone.
 * Com alimentação 3,3 V e saída em repouso ≈ Vcc/2, o valor ADC
 * (12 bits, 0–4095) fica em torno de 2048.
 * Ajuste medindo o valor médio em silêncio se necessário.
 */
#define ADC_DC_BIAS 2048

#define LED_RED_PIN 13   /* alerta de evento */
#define LED_BLUE_PIN 12  /* Wi-Fi conectando */
#define LED_GREEN_PIN 11 /* idle / standby   */

/* ============================================================
 * Amostragem ADC
 * ============================================================ */
#define SAMPLE_RATE_HZ 8000
#define DMA_BUFFER_SIZE 256 /* ~32 ms por janela de análise */

/* ============================================================
 * Detecção de Eventos
 * ============================================================ */
/**
 * Limiar RMS para disparo de evento (unidades ADC, 0–4095).
 * Calibre observando o heartbeat serial em ambiente silencioso
 * e defina como ~2× o valor de RMS em repouso.  Padrão: 150.
 */
#define THRESHOLD_RMS 150

/**
 * Tempo mínimo entre dois eventos consecutivos (antirepique).
 * Evita registros em rajada para um único som prolongado.
 */
#define MIN_EVENT_INTERVAL_MS 1000

/* ============================================================
 * Display OLED
 * ============================================================ */
/**
 * Valor máximo de RMS para escalar a barra de progresso a 100%.
 * Ajuste conforme a intensidade sonora do ambiente (típico: 400–800).
 */
#define RMS_MAX_DISPLAY 600

/** Período de atualização do display no modo idle (ms). */
#define DISPLAY_REFRESH_MS 150

/* ============================================================
 * Wi-Fi e Servidor de Notificação HTTP
 * ============================================================ */
#define WIFI_SSID "Marias"
#define WIFI_PASSWORD "88526352"
#define WIFI_CONNECT_TIMEOUT_MS 15000

/**
 * Endpoint que receberá HTTP POST com JSON:
 *   {"event":"sound_detected","rms":287.3,
 *    "timestamp_us":12543000,"device":"bitdoglab_v6.3"}
 */
#define NOTIFY_HOST "192.168.1.100"
#define NOTIFY_PORT 8080
#define NOTIFY_PATH "/notify"
#define HTTP_TIMEOUT_MS 4000

/* ============================================================
 * Log Circular de Eventos
 * ============================================================ */
#define MAX_LOG_EVENTS 30

typedef struct
{
    uint64_t timestamp_us;
    float rms;
} SoundEvent;

static SoundEvent s_event_log[MAX_LOG_EVENTS];
static int s_event_count = 0;
static int s_event_head = 0;

/* ============================================================
 * Buffers ADC / DMA
 * ============================================================ */
static uint16_t s_adc_buf[DMA_BUFFER_SIZE];
static int s_dma_chan = -1;
static dma_channel_config s_dma_cfg;

/* ============================================================
 * Contexto HTTP (lwIP raw TCP)
 * ============================================================ */
typedef struct
{
    struct tcp_pcb *pcb;
    volatile bool done;
    volatile bool error;
} HttpCtx;

static HttpCtx s_http;
static float s_pending_rms = 0.0f;
static uint64_t s_pending_ts = 0;

/* ============================================================
 * Estado Global
 * ============================================================ */
static bool s_wifi_connected = false;
static uint64_t s_last_event_us = 0;
static char s_wifi_ip[20] = "---";

/* ============================================================
 * Protótipos internos
 * ============================================================ */
/* ADC / DMA */
static void adc_dma_init(void);
static void adc_dma_capture(void);
static float calculate_rms(const uint16_t *buf, int n);
/* Log */
static void log_event(uint64_t ts, float rms);
static void print_event_log_serial(void);
/* LEDs */
static void leds_init(void);
static void led_set_alert(void);
static void led_set_idle(void);
/* Wi-Fi / HTTP */
static bool wifi_init_connect(void);
static void send_http_notification(float rms, uint64_t ts_us);
/* Display OLED */
static void display_draw_boot(const char *msg);
static void display_draw_idle(float rms);
static void display_draw_alert(float rms, uint64_t ts_us);

/* ============================================================
 * ADC + DMA
 * ============================================================ */

static void adc_dma_init(void)
{
    adc_init();
    adc_gpio_init(MIC_ADC_GPIO);
    adc_select_input(MIC_ADC_CHANNEL);

    /* Habilita FIFO com DMA data-request, 12 bits, sem bit de erro */
    adc_fifo_setup(true, true, 1, false, false);

    /* Divisor de clock:  f_adc = 48 MHz / (clkdiv + 1) = SAMPLE_RATE_HZ */
    float clkdiv = (float)(48000000 / SAMPLE_RATE_HZ) - 1.0f;
    adc_set_clkdiv(clkdiv);

    s_dma_chan = dma_claim_unused_channel(true);
    s_dma_cfg = dma_channel_get_default_config(s_dma_chan);
    channel_config_set_transfer_data_size(&s_dma_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&s_dma_cfg, false); /* sempre lê FIFO */
    channel_config_set_write_increment(&s_dma_cfg, true); /* avança no buffer */
    channel_config_set_dreq(&s_dma_cfg, DREQ_ADC);
}

static void adc_dma_capture(void)
{
    dma_channel_configure(s_dma_chan, &s_dma_cfg,
                          s_adc_buf, &adc_hw->fifo,
                          DMA_BUFFER_SIZE, true);
    adc_run(true);
    dma_channel_wait_for_finish_blocking(s_dma_chan);
    adc_run(false);
    adc_fifo_drain();
}

/* ============================================================
 * Processamento de Sinal
 * ============================================================ */

static float calculate_rms(const uint16_t *buf, int n)
{
    double sum_sq = 0.0;
    for (int i = 0; i < n; i++)
    {
        int32_t s = (int32_t)buf[i] - ADC_DC_BIAS;
        sum_sq += (double)(s * s);
    }
    return (float)sqrt(sum_sq / (double)n);
}

/* ============================================================
 * Log de Eventos
 * ============================================================ */

static void log_event(uint64_t ts, float rms)
{
    int idx = s_event_head % MAX_LOG_EVENTS;
    s_event_log[idx].timestamp_us = ts;
    s_event_log[idx].rms = rms;
    s_event_head++;
    if (s_event_count < MAX_LOG_EVENTS)
        s_event_count++;
}

static void print_event_log_serial(void)
{
    printf("\n┌────────────────────────────────────────┐\n");
    printf("│       LOG DE EVENTOS SONOROS           │\n");
    printf("├─────────────────┬──────────┬───────────┤\n");
    printf("│  Tempo (s.ms)   │   RMS    │   Seq.    │\n");
    printf("├─────────────────┼──────────┼───────────┤\n");
    int total = (s_event_count < MAX_LOG_EVENTS) ? s_event_count : MAX_LOG_EVENTS;
    int start = (s_event_count < MAX_LOG_EVENTS) ? 0 : (s_event_head % MAX_LOG_EVENTS);
    for (int i = 0; i < total; i++)
    {
        int idx = (start + i) % MAX_LOG_EVENTS;
        uint64_t ts = s_event_log[idx].timestamp_us;
        printf("│  %8lu.%03lu s │  %6.1f  │  #%6d  │\n",
               (uint32_t)(ts / 1000000ULL),
               (uint32_t)((ts % 1000000ULL) / 1000ULL),
               s_event_log[idx].rms, i + 1);
    }
    printf("└─────────────────┴──────────┴───────────┘\n\n");
}

/* ============================================================
 * LEDs
 * ============================================================ */

static void leds_init(void)
{
    const uint pins[] = {LED_RED_PIN, LED_BLUE_PIN, LED_GREEN_PIN};
    for (int i = 0; i < 3; i++)
    {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 0);
    }
}

static void led_set_alert(void)
{
    gpio_put(LED_RED_PIN, 1);
    gpio_put(LED_BLUE_PIN, 0);
    gpio_put(LED_GREEN_PIN, 0);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
}

static void led_set_idle(void)
{
    gpio_put(LED_RED_PIN, 0);
    gpio_put(LED_BLUE_PIN, 0);
    gpio_put(LED_GREEN_PIN, 1);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
}

/* ============================================================
 * Wi-Fi (CYW43 / lwIP)
 * ============================================================ */

static bool wifi_init_connect(void)
{
    display_draw_boot("Iniciando Wi-Fi...");
    printf("[Wi-Fi] Inicializando CYW43...\n");

    if (cyw43_arch_init())
    {
        printf("[Wi-Fi] ERRO: falha ao inicializar o chip.\n");
        display_draw_boot("Wi-Fi: ERRO INIT");
        sleep_ms(1500);
        return false;
    }
    cyw43_arch_enable_sta_mode();
    gpio_put(LED_BLUE_PIN, 1);

    display_draw_boot("Conectando Wi-Fi...");
    printf("[Wi-Fi] Conectando a \"%s\"...\n", WIFI_SSID);

    int ret = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID, WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK,
        WIFI_CONNECT_TIMEOUT_MS);

    gpio_put(LED_BLUE_PIN, 0);

    if (ret != 0)
    {
        printf("[Wi-Fi] ERRO codigo %d. Sem notificacoes remotas.\n", ret);
        display_draw_boot("Wi-Fi: FALHOU");
        sleep_ms(1500);
        return false;
    }

    snprintf(s_wifi_ip, sizeof(s_wifi_ip), "%s",
             ip4addr_ntoa(netif_ip4_addr(netif_default)));
    printf("[Wi-Fi] Conectado! IP: %s\n", s_wifi_ip);

    char ok_msg[40];
    snprintf(ok_msg, sizeof(ok_msg), "IP: %s", s_wifi_ip);
    display_draw_boot(ok_msg);
    sleep_ms(1500);
    return true;
}

/* ============================================================
 * Notificação HTTP (lwIP raw API)
 * ============================================================ */

static err_t http_on_recv(void *arg, struct tcp_pcb *pcb,
                          struct pbuf *p, err_t err)
{
    (void)arg;
    if (!p)
    {
        tcp_close(pcb);
        s_http.done = true;
        return ERR_OK;
    }
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t http_on_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    (void)arg;
    if (err != ERR_OK)
    {
        s_http.error = s_http.done = true;
        return err;
    }

    char body[160];
    int body_len = snprintf(body, sizeof(body),
                            "{\"event\":\"sound_detected\","
                            "\"rms\":%.1f,"
                            "\"timestamp_us\":%llu,"
                            "\"device\":\"bitdoglab_v6.3\"}",
                            s_pending_rms, (unsigned long long)s_pending_ts);

    char req[512];
    int req_len = snprintf(req, sizeof(req),
                           "POST %s HTTP/1.1\r\nHost: %s:%d\r\n"
                           "Content-Type: application/json\r\nContent-Length: %d\r\n"
                           "Connection: close\r\n\r\n%s",
                           NOTIFY_PATH, NOTIFY_HOST, NOTIFY_PORT, body_len, body);

    tcp_write(pcb, req, (u16_t)req_len, TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);
    return ERR_OK;
}

static void http_on_error(void *arg, err_t err)
{
    (void)arg;
    (void)err;
    s_http.error = s_http.done = true;
}

static void send_http_notification(float rms, uint64_t ts_us)
{
    if (!s_wifi_connected)
        return;
    s_pending_rms = rms;
    s_pending_ts = ts_us;
    s_http.done = s_http.error = false;

    ip_addr_t server_ip;
    if (!ip4addr_aton(NOTIFY_HOST, &server_ip))
        return;

    s_http.pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!s_http.pcb)
        return;
    tcp_recv(s_http.pcb, http_on_recv);
    tcp_err(s_http.pcb, http_on_error);

    if (tcp_connect(s_http.pcb, &server_ip, NOTIFY_PORT, http_on_connected) != ERR_OK)
        return;

    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    while (!s_http.done)
    {
        cyw43_arch_poll();
        if (to_ms_since_boot(get_absolute_time()) - t0 > HTTP_TIMEOUT_MS)
        {
            printf("[HTTP] Timeout — servidor nao respondeu.\n");
            tcp_abort(s_http.pcb);
            return;
        }
        sleep_ms(5);
    }
    printf("[HTTP] Notificacao %s (RMS=%.1f)\n",
           s_http.error ? "FALHOU" : "enviada", rms);
}

/* ============================================================
 * Funções de Display OLED
 * ============================================================ */

/**
 * display_draw_boot() — Tela de inicialização.
 *
 *  ┌────────────────────────────────┐
 *  │ ╔══════════════════════════╗   │
 *  │ ║   BITDOGLAB v6.3         ║   │
 *  │ ╚══════════════════════════╝   │
 *  │  Det. Sonoro por Ambiente      │
 *  │                                │
 *  │  > <mensagem dinâmica>         │
 *  └────────────────────────────────┘
 */
static void display_draw_boot(const char *msg)
{
    ssd1306_clear();

    /* Borda externa */
    ssd1306_draw_rect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_WHITE);

    /* Banner de título invertido */
    ssd1306_fill_rect(2, 2, SSD1306_WIDTH - 4, 14, SSD1306_WHITE);
    ssd1306_draw_string(6, 4, "BITDOGLAB v6.3", SSD1306_BLACK, FONT_SMALL);

    ssd1306_draw_string(4, 20, "Det. Sonoro Amb.", SSD1306_WHITE, FONT_SMALL);
    ssd1306_draw_string(4, 30, "Raspberry Pi PicoW", SSD1306_WHITE, FONT_SMALL);

    /* Separador */
    ssd1306_draw_hline(2, 42, SSD1306_WIDTH - 4, SSD1306_WHITE);

    /* Mensagem de status dinâmica */
    ssd1306_draw_string(4, 46, "> ", SSD1306_WHITE, FONT_SMALL);
    ssd1306_draw_string(16, 46, msg, SSD1306_WHITE, FONT_SMALL);

    ssd1306_show();
}

/**
 * display_draw_idle() — Tela principal de monitoramento contínuo.
 *
 *  ┌────────────────────────────────┐  px
 *  │ ))) DET. SONORO       #00012   │  y=0
 *  ├────────────────────────────────┤  y=11
 *  │ RMS [█████████░░░░░░░]   142   │  y=14
 *  ├────────────────────────────────┤  y=25
 *  │ AGUARDANDO...                  │  y=28
 *  │ Ult:00008.423s  R:214          │  y=38
 *  ├────────────────────────────────┤  y=49
 *  │ IP:192.168.1.42                │  y=52
 *  └────────────────────────────────┘
 */
static void display_draw_idle(float rms)
{
    ssd1306_clear();

    /* ─── Linha 0: cabeçalho ─── */
    ssd1306_draw_sound_icon(8, 6, true);
    ssd1306_draw_string(20, 1, "DET.SONORO", SSD1306_WHITE, FONT_SMALL);

    char cnt_buf[10];
    snprintf(cnt_buf, sizeof(cnt_buf), "#%05d", s_event_count);
    ssd1306_draw_string(80, 1, cnt_buf, SSD1306_WHITE, FONT_SMALL);

    ssd1306_draw_hline(0, 11, SSD1306_WIDTH, SSD1306_WHITE);

    /* ─── Linha 1: nível RMS ─── */
    ssd1306_draw_string(0, 14, "RMS", SSD1306_WHITE, FONT_SMALL);

    int pct = (int)((rms / (float)RMS_MAX_DISPLAY) * 100.0f);
    if (pct > 100)
        pct = 100;
    ssd1306_draw_progress_bar(20, 14, 86, 9, pct);

    char rms_buf[6];
    snprintf(rms_buf, sizeof(rms_buf), "%4.0f", rms);
    ssd1306_draw_string(108, 14, rms_buf, SSD1306_WHITE, FONT_SMALL);

    ssd1306_draw_hline(0, 25, SSD1306_WIDTH, SSD1306_WHITE);

    /* ─── Linha 2: status ─── */
    ssd1306_draw_string(0, 28, "AGUARDANDO...", SSD1306_WHITE, FONT_SMALL);

    /* ─── Linha 3: último evento ─── */
    if (s_event_count > 0)
    {
        int last_idx = (s_event_head - 1 + MAX_LOG_EVENTS) % MAX_LOG_EVENTS;
        uint64_t last_ts = s_event_log[last_idx].timestamp_us;
        float last_rms = s_event_log[last_idx].rms;
        char last_buf[28];
        snprintf(last_buf, sizeof(last_buf), "Ult:%05lu.%03lus R:%4.0f",
                 (uint32_t)(last_ts / 1000000ULL),
                 (uint32_t)((last_ts % 1000000ULL) / 1000ULL),
                 last_rms);
        ssd1306_draw_string(0, 38, last_buf, SSD1306_WHITE, FONT_SMALL);
    }
    else
    {
        ssd1306_draw_string(0, 38, "Sem eventos ainda", SSD1306_WHITE, FONT_SMALL);
    }

    ssd1306_draw_hline(0, 49, SSD1306_WIDTH, SSD1306_WHITE);

    /* ─── Linha 4: Wi-Fi ─── */
    if (s_wifi_connected)
    {
        char wifi_buf[22];
        snprintf(wifi_buf, sizeof(wifi_buf), "IP:%s", s_wifi_ip);
        ssd1306_draw_string(0, 52, wifi_buf, SSD1306_WHITE, FONT_SMALL);
    }
    else
    {
        ssd1306_draw_string(0, 52, "WiFi: desconectado", SSD1306_WHITE, FONT_SMALL);
    }

    ssd1306_show();
}

/**
 * display_draw_alert() — Tela de alerta ao detectar evento sonoro.
 * Usa borda dupla e cabeçalho invertido para máxima visibilidade.
 *
 *  ╔════════════════════════════════╗
 *  ║  !! EVENTO DETECTADO !!        ║  y=3  (fundo branco, texto preto)
 *  ╠════════════════════════════════╣
 *  ║  RMS: 287.3         #00013     ║  y=18 (fonte dupla para RMS)
 *  ║  [████████████████████████]    ║  y=36 (barra proporcional)
 *  ║  T: 00012.543 s                ║  y=47 (timestamp)
 *  ╠════════════════════════════════╣
 *  ║  HTTP: notificando...          ║  y=57 (status rede)
 *  ╚════════════════════════════════╝
 */
static void display_draw_alert(float rms, uint64_t ts_us)
{
    ssd1306_clear();

    /* Bordas duplas */
    ssd1306_draw_rect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, SSD1306_WHITE);
    ssd1306_draw_rect(2, 2, SSD1306_WIDTH - 4, SSD1306_HEIGHT - 4, SSD1306_WHITE);

    /* Cabeçalho invertido */
    ssd1306_fill_rect(3, 3, SSD1306_WIDTH - 6, 12, SSD1306_WHITE);
    ssd1306_draw_string(8, 4, "!! EVENTO DETECTADO !!", SSD1306_BLACK, FONT_SMALL);

    ssd1306_draw_hline(3, 16, SSD1306_WIDTH - 6, SSD1306_WHITE);

    /* Valor RMS em fonte média (scale=2) para destaque */
    char rms_str[10];
    snprintf(rms_str, sizeof(rms_str), "%.1f", rms);
    ssd1306_draw_string(4, 20, "RMS:", SSD1306_WHITE, FONT_SMALL);
    ssd1306_draw_string(28, 18, rms_str, SSD1306_WHITE, FONT_MEDIUM);

    /* Contador no canto direito */
    char cnt_str[10];
    snprintf(cnt_str, sizeof(cnt_str), "#%05d", s_event_count);
    ssd1306_draw_string(90, 20, cnt_str, SSD1306_WHITE, FONT_SMALL);

    /* Barra proporcional ao RMS */
    int pct = (int)((rms / (float)RMS_MAX_DISPLAY) * 100.0f);
    if (pct > 100)
        pct = 100;
    ssd1306_draw_progress_bar(4, 36, SSD1306_WIDTH - 8, 8, pct);

    /* Timestamp */
    uint32_t sec = (uint32_t)(ts_us / 1000000ULL);
    uint32_t ms = (uint32_t)((ts_us % 1000000ULL) / 1000ULL);
    char ts_str[24];
    snprintf(ts_str, sizeof(ts_str), "T: %05lu.%03lu s", sec, ms);
    ssd1306_draw_string(4, 47, ts_str, SSD1306_WHITE, FONT_SMALL);

    /* Rodapé: status HTTP */
    ssd1306_draw_hline(3, 56, SSD1306_WIDTH - 6, SSD1306_WHITE);
    ssd1306_draw_string(6, 57,
                        s_wifi_connected ? "HTTP: notificando..." : "WiFi: sem conexao",
                        SSD1306_WHITE, FONT_SMALL);

    ssd1306_show();
}

/* ============================================================
 * Ponto de Entrada
 * ============================================================ */
int main(void)
{
    stdio_init_all();
    sleep_ms(2000); /* aguarda enumeração USB-serial */

    printf("\n╔══════════════════════════════════════════╗\n");
    printf("║   Detector de Presença por Som Ambiente  ║\n");
    printf("║   BitDogLab v6.3 — Raspberry Pi Pico W  ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    /* ── Inicialização de periféricos ── */
    leds_init();
    ssd1306_init(); /* I2C1 + display OLED */
    adc_dma_init(); /* ADC + DMA contínuo  */

    display_draw_boot("Iniciando sistema...");
    sleep_ms(800);

    printf("[Config] Taxa de amostragem : %d Hz\n", SAMPLE_RATE_HZ);
    printf("[Config] Janela DMA         : %d amostras (~%d ms)\n",
           DMA_BUFFER_SIZE, (DMA_BUFFER_SIZE * 1000) / SAMPLE_RATE_HZ);
    printf("[Config] Limiar RMS         : %d unidades ADC\n", THRESHOLD_RMS);
    printf("[Config] Antirepique        : %d ms\n", MIN_EVENT_INTERVAL_MS);

    /* ── Conexão Wi-Fi ── */
    s_wifi_connected = wifi_init_connect();
    if (s_wifi_connected)
        printf("[Config] Servidor HTTP: http://%s:%d%s\n\n",
               NOTIFY_HOST, NOTIFY_PORT, NOTIFY_PATH);
    else
        printf("[Config] Wi-Fi ausente — modo offline ativo.\n\n");

    led_set_idle();
    display_draw_idle(0.0f);
    printf("[Sistema] Monitorando...\n\n");

    uint32_t last_display_ms = 0;
    uint32_t last_heartbeat_ms = 0;

    /* ══════════════════════════════════════════
     * Loop Principal
     * ══════════════════════════════════════════ */
    while (true)
    {

        /* 1 ── Captura DMA_BUFFER_SIZE amostras do microfone */
        adc_dma_capture();

        /* 2 ── Calcula RMS da janela atual */
        float rms = calculate_rms(s_adc_buf, DMA_BUFFER_SIZE);
        uint64_t now_us = to_us_since_boot(get_absolute_time());
        uint32_t now_ms = (uint32_t)(now_us / 1000ULL);

        /* 3 ── Verifica limiar + antirepique temporal */
        bool event_triggered =
            (rms >= (float)THRESHOLD_RMS) &&
            ((now_us - s_last_event_us) >= (uint64_t)MIN_EVENT_INTERVAL_MS * 1000ULL);

        if (event_triggered)
        {
            s_last_event_us = now_us;

            uint32_t sec = (uint32_t)(now_us / 1000000ULL);
            uint32_t ms = (uint32_t)((now_us % 1000000ULL) / 1000ULL);

            printf("┌─ [ALERTA] ──────────────────────────────\n");
            printf("│  Evento  : #%d\n", s_event_count + 1);
            printf("│  Timestamp: %lu.%03lu s\n", sec, ms);
            printf("│  RMS      : %.1f  (limiar=%d)\n", rms, THRESHOLD_RMS);
            printf("└─────────────────────────────────────────\n");

            /* Registra no log circular */
            log_event(now_us, rms);

            /* Feedback visual */
            led_set_alert();
            display_draw_alert(rms, now_us);

            /* Notificação HTTP (pode bloquear até HTTP_TIMEOUT_MS) */
            if (s_wifi_connected)
                send_http_notification(rms, now_us);

            /* Log serial a cada 5 eventos */
            if (s_event_count % 5 == 0)
                print_event_log_serial();

            /* Mantém tela + LED de alerta por 600 ms */
            sleep_ms(600);

            led_set_idle();

            /* Força atualização imediata da tela idle */
            last_display_ms = 0;
        }
        else
        {

            /* 4 ── Atualiza display idle periodicamente */
            if (now_ms - last_display_ms >= DISPLAY_REFRESH_MS)
            {
                display_draw_idle(rms);
                last_display_ms = now_ms;
            }

            /* 5 ── Heartbeat serial a cada 3 s */
            if (now_ms - last_heartbeat_ms >= 3000)
            {
                printf("[Monitor] RMS=%-7.1f  Eventos=%d  WiFi=%s\n",
                       rms, s_event_count,
                       s_wifi_connected ? s_wifi_ip : "OFF");
                last_heartbeat_ms = now_ms;
            }
        }

        /* 6 ── Mantém o stack Wi-Fi ativo (polling lwIP) */
        if (s_wifi_connected)
            cyw43_arch_poll();
    }

    /* Nunca alcançado */
    cyw43_arch_deinit();
    return 0;
}