#include <stdio.h>

int main()
{
    char nome[50];

    printf("Informe seu nome: ");
    fgets(nome, sizeof(nome), stdin);

    printf("Olá, %s!", nome);

    return 0;
}