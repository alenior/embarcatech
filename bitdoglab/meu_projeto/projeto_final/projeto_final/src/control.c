#include "control.h"
#include "pico/sync.h"
#include "pico/critical_section.h"

static volatile control_cmd_t g_pending = CONTROL_NONE;
static volatile unsigned int g_updated_at = 0;
static critical_section_t g_cs;

void control_init(void)
{
    critical_section_init(&g_cs);
}

void control_set_pending(control_cmd_t cmd, unsigned int updated_at)
{
    critical_section_enter_blocking(&g_cs);
    if (updated_at >= g_updated_at)
    {
        g_pending = cmd;
        g_updated_at = updated_at;
    }
    critical_section_exit(&g_cs);
}

control_cmd_t control_take_pending(unsigned int *updated_at)
{
    critical_section_enter_blocking(&g_cs);
    control_cmd_t cmd = g_pending;
    if (updated_at)
        *updated_at = g_updated_at;
    g_pending = CONTROL_NONE;
    critical_section_exit(&g_cs);
    return cmd;
}