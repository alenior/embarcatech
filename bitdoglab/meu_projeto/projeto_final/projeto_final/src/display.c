#include "display.h"
#include <stdio.h>

static char current[16] = "";

void display_init()
{
    printf("Display init\n");
}

void display_set_status(const char *status)
{
    snprintf(current, sizeof(current), "%s", status);
}

void display_task()
{
    // Aqui você encaixa TODO seu código OLED atual
    // (sem alterar lógica interna)
}