# Uso de um Joystick na BitDogLab

## Introdução
A BitDogLab é uma placa educacional que possui a Raspberry Pi Pico W integrada. Neste guia, exploraremos como conectar e utilizar o joystick analógico da BitDogLab.

## Materiais Necessários
- BitDogLab
- Fonte de alimentação (cabo USB para alimentação ou bateria integrada)

## Conexões do Joystick
Um joystick analógico geralmente possui cinco pinos principais:

1. **VCC** - Alimentação (+3.3V ou +5V)
2. **GND** - Terra
3. **vRx** - Saída do eixo X (Analógico)
4. **vRy** - Saída do eixo Y (Analógico)
5. **SW** - Botão de clique (Digital)

### Esquemático de Conexão com a BitDogLab
| Pino do Joystick | Pino do Raspberry Pi Pico W |
|------------------|----------------------------|
| VCC             | 3.3V                        |
| GND             | GND                         |
| vRx             | GP26 (ADC0)                 |
| vRy             | GP27 (ADC1)                 |
| SW              | GP22 (Entrada Digital)      |

## Código de Exemplo (Linguagem C)
Aqui está um código básico em C para ler os valores do joystick e exibi-los no console:

```c
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define vRx_PIN 26  // Pino ligado ao eixo X (ADC0)
#define vRy_PIN 27  // Pino ligado ao eixo Y (ADC1)
#define SW 22   // Pino do botão (Digital)

int main() {
    stdio_init_all();
    adc_init();
    adc_gpio_init(vRx_PIN);
    adc_gpio_init(vRy_PIN);
    gpio_init(SW);
    gpio_set_dir(SW, GPIO_IN);
    gpio_pull_up(SW);
    
    while (1) {
        adc_select_input(0); // Seleciona o canal ADC0 (vRx)
        uint16_t x_value = adc_read();
        
        adc_select_input(1); // Seleciona o canal ADC1 (vRy)
        uint16_t y_value = adc_read();
        
        int button_state = gpio_get(SW);
        
        printf("X: %d, Y: %d, Button: %s\n", x_value, y_value, button_state == 0 ? "Pressionado" : "Solto");
        sleep_ms(100);
    }
}
```

## Explicação do Código
- **Leitura dos valores analógicos**: O joystick retorna valores entre 0 e 4095 (ADC de 12 bits do Pico W) para os eixos X e Y.
- **Detecção do botão**: O botão do joystick é lido como um valor digital (0 = pressionado, 1 = solto).
- **Loop infinito**: O código lê continuamente os valores e os imprime no console.

## Testando o Circuito
1. Conecte a BitDogLab ao computador.
2. Compile e carregue o código usando o SDK do Raspberry Pi.
3. Observe os valores no monitor serial conforme movimenta o joystick.

## Conclusão
Este guia mostrou como conectar e ler os valores de um joystick analógico usando a BitDogLab em linguagem C. Esse conhecimento pode ser aplicado em projetos como robôs, controles remotos e jogos interativos.

---
Esperamos que este tutorial tenha sido útil! 🚀
