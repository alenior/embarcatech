#include "utils.h"
#include "pico/time.h"

uint32_t get_timestamp_ms()
{
    return to_ms_since_boot(get_absolute_time());
}