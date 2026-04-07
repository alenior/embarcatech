#include <stdio.h>
#include "pir.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"

static volatile bool triggered = false;

static void pir_irq(uint gpio, uint32_t events)
{
    triggered = true;
}

void pir_init()
{
    gpio_init(PIR_PIN);
    gpio_set_dir(PIR_PIN, GPIO_IN);

    gpio_set_irq_enabled_with_callback(
        PIR_PIN,
        GPIO_IRQ_EDGE_RISE,
        true,
        &pir_irq
    );
    printf("PIR init\n");
}

bool pir_get()
{
    uint32_t irq = save_and_disable_interrupts();

    bool val = triggered;
    triggered = false;

    restore_interrupts(irq);

    return val;
}