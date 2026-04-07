#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

void wifi_init();
void wifi_task();
bool wifi_is_connected();

#endif