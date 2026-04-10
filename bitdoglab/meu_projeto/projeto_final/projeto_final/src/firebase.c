#include "utils.h"

#include "firebase.h"
#include "config.h"
#include "wifi.h"
#include "control.h"

#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"

#include "pico/time.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#define QUEUE_SIZE 10
#define REQUEST_BUFFER_SIZE 800
#define RETRY_DELAY_US 3000000
#define AUTH_FAIL_BACKOFF_US 30000000
#define CONTROL_POLL_US 2000000
#define RESPONSE_BUFFER_SIZE 1200

typedef struct
{
    char json[192];
    char path[128];
} event_t;

typedef enum
{
    FB_OP_NONE = 0,
    FB_OP_SEND_EVENT,
    FB_OP_FETCH_CONTROL
} fb_op_t;

typedef enum
{
    FB_IDLE = 0,
    FB_DNS_WAIT,
    FB_CONNECTING,
    FB_WAIT_RESPONSE
} fb_state_t;

static event_t queue[QUEUE_SIZE];
static int head = 0;
static int tail = 0;

static fb_state_t fb_state = FB_IDLE;
static fb_op_t fb_op = FB_OP_NONE;

static struct altcp_pcb *fb_pcb = NULL;
static struct altcp_tls_config *fb_tls_cfg = NULL;
static ip_addr_t fb_ip;

static absolute_time_t next_retry = {0};
static absolute_time_t last_wifi_wait_log = {0};
static absolute_time_t next_control_poll = {0};

static int fb_http_status = 0;
static bool fb_headers_done = false;
static char fb_resp[RESPONSE_BUFFER_SIZE];
static size_t fb_resp_len = 0;
static unsigned int last_control_updated_at = 0;

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

static const char *skip_ws(const char *p)
{
    while (p && *p && isspace((unsigned char)*p))
        p++;
    return p;
}

static bool json_find_key(const char *json, const char *key, const char **value_start)
{
    if (!json || !key || !value_start)
        return false;

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = json;
    while ((p = strstr(p, pattern)) != NULL)
    {
        p += strlen(pattern);
        p = skip_ws(p);
        if (*p != ':')
            continue;
        p++;
        p = skip_ws(p);
        *value_start = p;
        return true;
    }
    return false;
}

static bool json_get_string(const char *json, const char *key, char *out, size_t out_sz)
{
    const char *v = NULL;
    if (!json_find_key(json, key, &v))
        return false;
    if (*v != '"')
        return false;
    v++;

    size_t i = 0;
    while (*v && *v != '"' && i + 1 < out_sz)
    {
        if (*v == '\\' && v[1] != '\0')
            v++;
        out[i++] = *v++;
    }
    if (*v != '"')
        return false;
    out[i] = '\0';
    return true;
}

static bool json_get_uint(const char *json, const char *key, unsigned int *out)
{
    const char *v = NULL;
    if (!json_find_key(json, key, &v))
        return false;

    char *endptr = NULL;
    unsigned long ul = strtoul(v, &endptr, 10);
    if (v == endptr)
        return false;
    *out = (unsigned int)ul;
    return true;
}

static bool parse_control_payload(const char *json, control_cmd_t *cmd, unsigned int *updated_at)
{
    if (!json || !cmd || !updated_at)
        return false;

    char desired[24] = {0};
    unsigned int ts = 0;

    if (!json_get_string(json, "desired_state", desired, sizeof(desired)))
        return false;
    if (!json_get_uint(json, "updated_at", &ts))
        return false;

    if (strcmp(desired, "ARMED") == 0)
    {
        *cmd = CONTROL_ARM;
    }
    else if (strcmp(desired, "DISARMED") == 0)
    {
        *cmd = CONTROL_DISARM;
    }
    else
    {
        return false;
    }

    *updated_at = ts;
    return true;
}

void firebase_log(const char *state, const char *event)
{
    char json[192];
    uint32_t ts = get_timestamp_ms();

    snprintf(json, sizeof(json),
             "{\"state\":\"%s\",\"event\":\"%s\",\"ts\":%u,"
             "\"device_id\":\"%s\",\"fw_version\":\"%s\"}",
             state, event, ts, DEVICE_ID, FW_VERSION);

    char path[128];
    snprintf(path, sizeof(path), FB_EVENTS_PATH "/%u", ts);
    enqueue(json, path);
}

void firebase_set_status(const char *status)
{
    char json[192];
    uint32_t ts = get_timestamp_ms();

    snprintf(json, sizeof(json),
             "{\"status\":\"%s\",\"ts\":%u,"
             "\"device_id\":\"%s\",\"fw_version\":\"%s\",\"wifi_ok\":%s}",
             status, ts, DEVICE_ID, FW_VERSION, wifi_is_connected() ? "true" : "false");

    enqueue(json, FB_STATUS_PATH);
}

static void firebase_reset_response_buffer(void)
{
    fb_headers_done = false;
    fb_resp_len = 0;
    fb_resp[0] = '\0';
    fb_http_status = 0;
}

static void firebase_reset_connection(void)
{
    if (fb_pcb != NULL)
    {
        altcp_arg(fb_pcb, NULL);
        altcp_recv(fb_pcb, NULL);
        altcp_err(fb_pcb, NULL);

        err_t c = altcp_close(fb_pcb);
        if (c != ERR_OK)
            altcp_abort(fb_pcb);

        fb_pcb = NULL;
    }

    fb_state = FB_IDLE;
    fb_op = FB_OP_NONE;
    firebase_reset_response_buffer();
}

static void firebase_schedule_retry(void)
{
    next_retry = make_timeout_time_us(RETRY_DELAY_US);
    firebase_reset_connection();
}

static err_t firebase_on_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err)
{
    (void)arg;

    if (err != ERR_OK)
    {
        printf("Firebase recv erro: %d\n", err);
        if (fb_op == FB_OP_SEND_EVENT)
            firebase_schedule_retry();
        else
            firebase_reset_connection();
        return ERR_OK;
    }

    if (p == NULL)
    {
        if (fb_op == FB_OP_SEND_EVENT)
        {
            if (fb_http_status == 200 || fb_http_status == 204)
            {
                printf("Firebase envio OK path=%s status=%d\n", queue[head].path, fb_http_status);
                pop_event();
                firebase_reset_connection();
                return ERR_OK;
            }

            if (fb_http_status == 401 || fb_http_status == 403)
            {
                printf("Firebase auth falhou (status=%d) path=%s\n", fb_http_status, queue[head].path);
                // evita fila lotada em loop
                pop_event();
                next_retry = make_timeout_time_us(AUTH_FAIL_BACKOFF_US);
                firebase_reset_connection();
                return ERR_OK;
            }

            printf("Firebase HTTP falhou status=%d path=%s\n", fb_http_status, queue[head].path);
            firebase_schedule_retry();
            return ERR_OK;
        }

        if (fb_op == FB_OP_FETCH_CONTROL)
        {
            if (fb_http_status == 200 && fb_resp_len > 0)
            {
                control_cmd_t cmd = CONTROL_NONE;
                unsigned int updated_at = 0;
                if (parse_control_payload(fb_resp, &cmd, &updated_at))
                {
                    if (updated_at > last_control_updated_at)
                    {
                        last_control_updated_at = updated_at;
                        control_set_pending(cmd, updated_at);
                        printf("Control atualizado: cmd=%d ts=%u\n", (int)cmd, updated_at);
                    }
                }
            }
            else if (fb_http_status == 401 || fb_http_status == 403)
            {
                printf("Control auth falhou (status=%d)\n", fb_http_status);
                next_control_poll = make_timeout_time_us(AUTH_FAIL_BACKOFF_US);
            }

            firebase_reset_connection();
            return ERR_OK;
        }

        firebase_reset_connection();
        return ERR_OK;
    }

    char chunk[450];
    u16_t copy_len = p->tot_len < (u16_t)(sizeof(chunk) - 1) ? p->tot_len : (u16_t)(sizeof(chunk) - 1);
    pbuf_copy_partial(p, chunk, copy_len, 0);
    chunk[copy_len] = '\0';

    if (fb_http_status == 0)
    {
        int code = 0;
        if (sscanf(chunk, "HTTP/%*d.%*d %d", &code) == 1)
        {
            fb_http_status = code;
            printf("Firebase resposta HTTP=%d\n", fb_http_status);
        }
    }

    const char *body_ptr = chunk;
    if (!fb_headers_done)
    {
        char *sep = strstr(chunk, "\r\n\r\n");
        if (sep)
        {
            fb_headers_done = true;
            body_ptr = sep + 4;
        }
        else
        {
            body_ptr = NULL;
        }
    }

    if (body_ptr && *body_ptr)
    {
        size_t body_len = strlen(body_ptr);
        if (fb_resp_len + body_len + 1 < sizeof(fb_resp))
        {
            memcpy(&fb_resp[fb_resp_len], body_ptr, body_len);
            fb_resp_len += body_len;
            fb_resp[fb_resp_len] = '\0';
        }
    }

    altcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void firebase_on_err(void *arg, err_t err)
{
    (void)arg;
    printf("Firebase conexão erro: %d\n", err);
    fb_pcb = NULL;
    fb_state = FB_IDLE;
    fb_op = FB_OP_NONE;
    next_retry = make_timeout_time_us(RETRY_DELAY_US);
}

static err_t firebase_on_connected(void *arg, struct altcp_pcb *pcb, err_t err)
{
    (void)arg;

    if (err != ERR_OK)
    {
        printf("Firebase connect falhou: %d\n", err);
        if (fb_op == FB_OP_SEND_EVENT)
            firebase_schedule_retry();
        else
            firebase_reset_connection();
        return err;
    }

    firebase_reset_response_buffer();

    char request[REQUEST_BUFFER_SIZE];
    int request_len = 0;

    if (fb_op == FB_OP_SEND_EVENT)
    {
        const char *json = queue[head].json;
        const char *path = queue[head].path;
        int len = (int)strlen(json);

        request_len = snprintf(
            request, sizeof(request),
            "PUT /%s.json?auth=%s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Accept: application/json\r\n"
            "User-Agent: PicoW/BitDogLab\r\n"
            "Connection: close\r\n\r\n"
            "%s",
            path, FIREBASE_AUTH_TOKEN, FIREBASE_HOST, len, json);
    }
    else
    {
        request_len = snprintf(
            request, sizeof(request),
            "GET /%s.json?auth=%s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Accept: application/json\r\n"
            "User-Agent: PicoW/BitDogLab\r\n"
            "Connection: close\r\n\r\n",
            FB_CONTROL_PATH, FIREBASE_AUTH_TOKEN, FIREBASE_HOST);
    }

    if (request_len <= 0 || request_len >= (int)sizeof(request))
    {
        printf("Firebase request inválida/grande\n");
        if (fb_op == FB_OP_SEND_EVENT)
            firebase_schedule_retry();
        else
            firebase_reset_connection();
        return ERR_BUF;
    }

    err_t wr = altcp_write(pcb, request, (u16_t)request_len, TCP_WRITE_FLAG_COPY);
    if (wr != ERR_OK)
    {
        printf("Firebase altcp_write erro: %d\n", wr);
        if (fb_op == FB_OP_SEND_EVENT)
            firebase_schedule_retry();
        else
            firebase_reset_connection();
        return wr;
    }

    err_t out = altcp_output(pcb);
    if (out != ERR_OK)
    {
        printf("Firebase altcp_output erro: %d\n", out);
        if (fb_op == FB_OP_SEND_EVENT)
            firebase_schedule_retry();
        else
            firebase_reset_connection();
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
        if (fb_op == FB_OP_SEND_EVENT)
            firebase_schedule_retry();
        else
            firebase_reset_connection();
        return;
    }

    fb_ip = *ipaddr;

    if (fb_tls_cfg == NULL)
    {
        fb_tls_cfg = altcp_tls_create_config_client(NULL, 0);
        if (fb_tls_cfg == NULL)
        {
            printf("Firebase TLS config falhou\n");
            if (fb_op == FB_OP_SEND_EVENT)
                firebase_schedule_retry();
            else
                firebase_reset_connection();
            return;
        }
    }

    fb_pcb = altcp_tls_new(fb_tls_cfg, IP_GET_TYPE(&fb_ip));
    if (fb_pcb == NULL)
    {
        printf("Firebase altcp_tls_new falhou\n");
        if (fb_op == FB_OP_SEND_EVENT)
            firebase_schedule_retry();
        else
            firebase_reset_connection();
        return;
    }

    altcp_arg(fb_pcb, NULL);
    altcp_recv(fb_pcb, firebase_on_recv);
    altcp_err(fb_pcb, firebase_on_err);

    err_t c = altcp_connect(fb_pcb, &fb_ip, FIREBASE_PORT, firebase_on_connected);
    if (c != ERR_OK)
    {
        printf("Firebase altcp_connect erro: %d\n", c);
        if (fb_op == FB_OP_SEND_EVENT)
            firebase_schedule_retry();
        else
            firebase_reset_connection();
        return;
    }

    fb_state = FB_CONNECTING;
}

void firebase_init(void)
{
    fb_state = FB_IDLE;
    fb_op = FB_OP_NONE;
    head = tail = 0;
    fb_pcb = NULL;
    fb_tls_cfg = NULL;
    fb_http_status = 0;
    fb_headers_done = false;
    fb_resp_len = 0;
    fb_resp[0] = '\0';
    next_retry = make_timeout_time_us(0);
    next_control_poll = make_timeout_time_us(1500000);

    printf("Firebase init\n");
}

void firebase_task(void)
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

    if (fb_state != FB_IDLE)
        return;
    if (absolute_time_diff_us(get_absolute_time(), next_retry) > 0)
        return;
    if (head == tail)
        return;

    fb_op = FB_OP_SEND_EVENT;
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

void firebase_fetch_control(void)
{
    if (!wifi_is_connected())
        return;
    if (fb_state != FB_IDLE)
        return;

    if (absolute_time_diff_us(get_absolute_time(), next_control_poll) > 0)
        return;
    next_control_poll = make_timeout_time_us(CONTROL_POLL_US);

    fb_op = FB_OP_FETCH_CONTROL;

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

    printf("Control dns_gethostbyname erro: %d\n", d);
    firebase_reset_connection();
}