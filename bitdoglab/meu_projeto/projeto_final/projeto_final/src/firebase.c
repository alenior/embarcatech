#include "utils.h"

#include "firebase.h"
#include "config.h"
#include "wifi.h"

#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"

#include "pico/time.h"

#include <stdio.h>
#include <string.h>

#define QUEUE_SIZE 10
#define REQUEST_BUFFER_SIZE 512
#define RETRY_DELAY_US 3000000

typedef struct
{
    char json[128];
    char path[64];
} event_t;

static event_t queue[QUEUE_SIZE];
static int head = 0;
static int tail = 0;

typedef enum
{
    FB_IDLE,
    FB_DNS_WAIT,
    FB_CONNECTING,
    FB_WAIT_RESPONSE
} fb_state_t;

static fb_state_t fb_state = FB_IDLE;
static struct tcp_pcb *fb_pcb = NULL;
static ip_addr_t fb_ip;
static absolute_time_t next_retry = {0};
static int fb_http_status = 0;
static absolute_time_t last_wifi_wait_log = {0};

static int queue_count(void)
{
    if (tail >= head)
        return tail - head;

    return QUEUE_SIZE - head + tail;
}

static void enqueue(const char *json, const char *path)
{
    int next = (tail + 1) % QUEUE_SIZE;

    if (next == head)
    {
        printf("Firebase queue cheia, descartando evento\n");
        return;
    }

    snprintf(queue[tail].json, sizeof(queue[tail].json), "%s", json);
    snprintf(queue[tail].path, sizeof(queue[tail].path), "%s", path);
    tail = next;
}

static void pop_event(void)
{
    if (head != tail)
        head = (head + 1) % QUEUE_SIZE;
}

void firebase_log(const char *state, const char *event)
{
    char json[128];
    uint32_t ts = get_timestamp_ms();

    snprintf(json, sizeof(json),
             "{\"state\":\"%s\",\"event\":\"%s\",\"ts\":%u}",
             state, event, ts);

    char path[64];
    snprintf(path, sizeof(path), "alarm/%u", ts);
    enqueue(json, path);
}

void firebase_set_status(const char *status)
{
    char json[128];
    uint32_t ts = get_timestamp_ms();

    snprintf(json, sizeof(json),
             "{\"status\":\"%s\",\"ts\":%u}",
             status, ts);

    enqueue(json, "alarm_status/current");
}

static void firebase_reset_connection(void)
{
    if (fb_pcb != NULL)
    {
        tcp_arg(fb_pcb, NULL);
        tcp_recv(fb_pcb, NULL);
        tcp_err(fb_pcb, NULL);
        tcp_poll(fb_pcb, NULL, 0);

        err_t c = tcp_close(fb_pcb);
        if (c != ERR_OK)
            tcp_abort(fb_pcb);

        fb_pcb = NULL;
    }

    fb_state = FB_IDLE;
}

static void firebase_schedule_retry(void)
{
    next_retry = make_timeout_time_us(RETRY_DELAY_US);
    firebase_reset_connection();
}

static err_t firebase_on_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    (void)arg;

    if (err != ERR_OK)
    {
        printf("Firebase recv erro: %d\n", err);
        firebase_schedule_retry();
        return ERR_OK;
    }

    if (p == NULL)
    {
        if (fb_http_status == 200 || fb_http_status == 204)
        {
            printf("Firebase envio OK path=%s status=%d\n", queue[head].path, fb_http_status);
            pop_event();
            firebase_reset_connection();
            return ERR_OK;
        }

        printf("Firebase HTTP falhou status=%d path=%s\n", fb_http_status, queue[head].path);
        firebase_schedule_retry();
        return ERR_OK;
    }

    if (fb_http_status == 0)
    {
        char line[48] = {0};
        u16_t copy_len = p->tot_len < (u16_t)(sizeof(line) - 1) ? p->tot_len : (u16_t)(sizeof(line) - 1);
        pbuf_copy_partial(p, line, copy_len, 0);

        int code = 0;
        if (sscanf(line, "HTTP/%*d.%*d %d", &code) == 1)
        {
            fb_http_status = code;
            printf("Firebase resposta HTTP=%d\n", fb_http_status);
        }
    }

    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void firebase_on_err(void *arg, err_t err)
{
    (void)arg;
    printf("Firebase conexão erro: %d\n", err);
    fb_pcb = NULL;
    fb_state = FB_IDLE;
    next_retry = make_timeout_time_us(RETRY_DELAY_US);
}

static err_t firebase_on_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    (void)arg;

    if (err != ERR_OK)
    {
        printf("Firebase connect falhou: %d\n", err);
        firebase_schedule_retry();
        return err;
    }

    fb_http_status = 0;

    char request[REQUEST_BUFFER_SIZE];
    const char *json = queue[head].json;
    const char *path = queue[head].path;
    int len = (int)strlen(json);

    int request_len = snprintf(
        request,
        sizeof(request),
        "PUT /%s.json HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Accept: application/json\r\n"
        "User-Agent: PicoW/BitDogLab\r\n"
        "Connection: close\r\n\r\n"
        "%s",
        path,
        FIREBASE_HOST,
        len,
        json);

    if (request_len <= 0 || request_len >= (int)sizeof(request))
    {
        printf("Firebase request muito grande\n");
        firebase_schedule_retry();
        return ERR_BUF;
    }

    err_t wr = tcp_write(tpcb, request, (u16_t)request_len, TCP_WRITE_FLAG_COPY);
    if (wr != ERR_OK)
    {
        printf("Firebase tcp_write erro: %d\n", wr);
        firebase_schedule_retry();
        return wr;
    }

    err_t out = tcp_output(tpcb);
    if (out != ERR_OK)
    {
        printf("Firebase tcp_output erro: %d\n", out);
        firebase_schedule_retry();
        return out;
    }

    fb_state = FB_WAIT_RESPONSE;
    return ERR_OK;
}

static void firebase_dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg)
{
    (void)name;
    (void)callback_arg;

    if (ipaddr == NULL)
    {
        printf("Firebase DNS falhou\n");
        firebase_schedule_retry();
        return;
    }

    fb_ip = *ipaddr;

    fb_pcb = tcp_new_ip_type(IP_GET_TYPE(&fb_ip));
    if (fb_pcb == NULL)
    {
        printf("Firebase tcp_new falhou\n");
        firebase_schedule_retry();
        return;
    }

    tcp_arg(fb_pcb, NULL);
    tcp_recv(fb_pcb, firebase_on_recv);
    tcp_err(fb_pcb, firebase_on_err);

    err_t c = tcp_connect(fb_pcb, &fb_ip, FIREBASE_PORT, firebase_on_connected);
    if (c != ERR_OK)
    {
        printf("Firebase tcp_connect erro: %d\n", c);
        firebase_schedule_retry();
        return;
    }

    fb_state = FB_CONNECTING;
}

void firebase_task()
{
    if (!wifi_is_connected())
    {
        if (absolute_time_diff_us(last_wifi_wait_log, get_absolute_time()) > 3000000)
        {
            last_wifi_wait_log = get_absolute_time();
            printf("Firebase aguardando WiFi...\n");
        }

        firebase_reset_connection();
        return;
    }

    if (head == tail)
        return;

    if (fb_state != FB_IDLE)
        return;

    if (absolute_time_diff_us(get_absolute_time(), next_retry) > 0)
        return;

    printf("Firebase enviando path=%s (%d na fila)\n", queue[head].path, queue_count());

    err_t d = dns_gethostbyname(FIREBASE_HOST, &fb_ip, firebase_dns_found, NULL);
    if (d == ERR_OK)
    {
        firebase_dns_found(FIREBASE_HOST, &fb_ip, NULL);
        return;
    }

    if (d == ERR_INPROGRESS)
    {
        fb_state = FB_DNS_WAIT;
        return;
    }

    printf("Firebase dns_gethostbyname erro: %d\n", d);
    next_retry = make_timeout_time_us(RETRY_DELAY_US);
}