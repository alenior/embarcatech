#ifndef CONTROL_H
#define CONTROL_H

typedef enum
{
    CONTROL_NONE = 0,
    CONTROL_ARM,
    CONTROL_DISARM
} control_cmd_t;

void control_init(void);
void control_set_pending(control_cmd_t cmd, unsigned int updated_at);
control_cmd_t control_take_pending(unsigned int *updated_at);

#endif