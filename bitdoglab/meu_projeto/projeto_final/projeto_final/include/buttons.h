#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdbool.h>

// Inicialização
void buttons_init(void);

// Leitura dos botões
bool button_a_pressed(void);
bool button_b_pressed(void);

// Ações lógicas
bool buttons_ok(void);
bool buttons_back(void);

#endif