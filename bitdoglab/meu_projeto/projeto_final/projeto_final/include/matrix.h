#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>

typedef enum
{
    MATRIX_PATTERN_OFF = 0,
    MATRIX_PATTERN_ARMED,
    MATRIX_PATTERN_DISARMED,
    MATRIX_PATTERN_TRIGGERED
} matrix_pattern_t;

void matrix_init(void);
void matrix_set_pattern(matrix_pattern_t pattern);

#endif