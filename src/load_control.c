#include "load_control.h"
#include "iset_dac.h"
#include "internal_isen.h"

void load_control_task(void) {

    // LED
    rcc_enable_peripheral_clock(LED_GREEN_GPIO_CLOCK);
    gpio_write(LED_GREEN_GPIO, HIGH);
    gpio_set_mode(LED_GREEN_GPIO, GPIO_MODE_OUTPUT);

    // EN L
    rcc_enable_peripheral_clock(LOAD_EN_L_GPIO_CLOCK);
    gpio_set_mode(LOAD_EN_L_GPIO, GPIO_MODE_OUTPUT);

    // EN R
    rcc_enable_peripheral_clock(LOAD_EN_R_GPIO_CLOCK);
    gpio_set_mode(LOAD_EN_R_GPIO, GPIO_MODE_OUTPUT);
    
    iset_dac_init();
    internal_isen_init();

    while (1) {

        kernel_yield();
    }
}

void load_set_current(uint16_t current_ma) {

    iset_dac_set_current(current_ma);
}

void load_set_enable(bool state) {

    gpio_write(LOAD_EN_L_GPIO, state);
    gpio_write(LOAD_EN_R_GPIO, state);
    gpio_write(LED_GREEN_GPIO, !state);
}