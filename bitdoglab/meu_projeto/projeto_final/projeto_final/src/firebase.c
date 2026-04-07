#include "utils.h"

#include "firebase.h"
#include "config.h"
#include "wifi.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include <stdio.h>
#include <string.h>

#define QUEUE_SIZE 10

typedef struct
{
    char json[128];
    char path[64];
} event_t;

static event_t queue[QUEUE_SIZE];
static int head = 0, tail = 0;

static void enqueue(const char *json, const char *path)
{
    int next = (tail + 1) % QUEUE_SIZE;

    if (next == head)
        return;

    strcpy(queue[tail].json, json);
    strcpy(queue[tail].path, path);

    tail = next;
}

void firebase_log(const char *state, const char *event)
{
    char json[128];
    uint32_t ts = get_timestamp_ms();

    snprintf(json, sizeof(json),
             "{\"state\":\"%s\",\"event\":\"%s\",\"ts\":%u}",
             state, event, ts);

    char path[64];
    sprintf(path, "alarm/%u", ts);

    enqueue(json, path);
}

void firebase_task()
{
    if (!wifi_is_connected())
        return;

    if (head == tail)
        return;

    // (versão simplificada: envia 1 por ciclo)
    // aqui você pode reaproveitar seu TCP já funcional

    printf("Enviando evento Firebase\n");

    head = (head + 1) % QUEUE_SIZE;
}