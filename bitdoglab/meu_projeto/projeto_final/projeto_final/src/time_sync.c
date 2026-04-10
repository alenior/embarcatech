#include "time_sync.h"
#include "pico/time.h"

// Implementação mínima inicial:
// - Fase 1: retorna inválido (0) até integrar NTP real.
// - Fase 2: integrar SNTP lwIP e manter offset epoch.

static bool g_valid = false;
static int64_t g_epoch_offset_ms = 0; // epoch_ms - boot_ms

void time_sync_init(void)
{
    g_valid = false;
    g_epoch_offset_ms = 0;
}

void time_sync_task(void)
{
    // TODO: integrar SNTP aqui
    // Quando sincronizar:
    // g_epoch_offset_ms = epoch_ms_recebido - to_ms_since_boot(get_absolute_time());
    // g_valid = true;
}

bool time_sync_is_valid(void) { return g_valid; }

uint64_t time_sync_unix_ms(void)
{
    if (!g_valid)
        return 0;
    uint64_t boot_ms = to_ms_since_boot(get_absolute_time());
    return (uint64_t)((int64_t)boot_ms + g_epoch_offset_ms);
}