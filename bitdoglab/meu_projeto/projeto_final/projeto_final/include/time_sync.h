#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <stdbool.h>
#include <stdint.h>

void time_sync_init(void);
void time_sync_task(void);
bool time_sync_is_valid(void);
uint64_t time_sync_unix_ms(void); // epoch ms (0 se inválido)

#endif