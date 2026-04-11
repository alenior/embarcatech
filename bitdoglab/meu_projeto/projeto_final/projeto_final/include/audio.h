#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

void audio_init();
bool audio_detect();
void audio_rearm(void);

#endif