#include "control.h"

static control_cmd_t g_pending_cmd = CONTROL_NONE;
static unsigned int g_pending_updated_at = 0;
static bool g_has_pending = false;

void control_init(void)
{
    g_pending_cmd = CONTROL_NONE;
    g_pending_updated_at = 0;
    g_has_pending = false;
}

void control_set_pending(control_cmd_t cmd, unsigned int updated_at)
{
    if (cmd == CONTROL_NONE)
        return;

    g_pending_cmd = cmd;
    g_pending_updated_at = updated_at;
    g_has_pending = true;
}

bool control_take_pending(control_cmd_t *cmd, unsigned int *updated_at)
{
    if (!g_has_pending || !cmd || !updated_at)
        return false;

    *cmd = g_pending_cmd;
    *updated_at = g_pending_updated_at;

    g_pending_cmd = CONTROL_NONE;
    g_pending_updated_at = 0;
    g_has_pending = false;

    return true;
}