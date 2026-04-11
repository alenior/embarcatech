#ifndef CONTROL_H
#define CONTROL_H

#include <stdbool.h>

typedef enum
{
    CONTROL_NONE = 0,
    CONTROL_ARM,
    CONTROL_DISARM
} control_cmd_t;

void control_init(void);
void control_set_pending(control_cmd_t cmd, unsigned int updated_at);
bool control_take_pending(control_cmd_t *cmd, unsigned int *updated_at);

#endif