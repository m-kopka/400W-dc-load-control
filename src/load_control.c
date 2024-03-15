#include "load_control.h"
#include "iset_dac.h"
#include "internal_isen.h"
#include "vi_sense.h"
#include "cmd_interface/cmd_spi_driver.h"

static uint32_t discharge_voltage = 0;
bool enabled = false;

void load_control_task(void) {

    // LED
    rcc_enable_peripheral_clock(LED_GREEN_GPIO_CLOCK);
    gpio_write(LED_GREEN_GPIO, HIGH);
    gpio_set_mode(LED_GREEN_GPIO, GPIO_MODE_OUTPUT);

    rcc_enable_peripheral_clock(LED_RED_GPIO_CLOCK);
    gpio_write(LED_RED_GPIO, HIGH);
    gpio_set_mode(LED_RED_GPIO, GPIO_MODE_OUTPUT);

    // EN L
    rcc_enable_peripheral_clock(LOAD_EN_L_GPIO_CLOCK);
    gpio_set_mode(LOAD_EN_L_GPIO, GPIO_MODE_OUTPUT);

    // EN R
    rcc_enable_peripheral_clock(LOAD_EN_R_GPIO_CLOCK);
    gpio_set_mode(LOAD_EN_R_GPIO, GPIO_MODE_OUTPUT);
    
    iset_dac_init();
    internal_isen_init();

    uint32_t vi_sense_stack[64];
    kernel_create_task(vi_sense_task, vi_sense_stack, sizeof(vi_sense_stack), 100);

    while (1) {

        if (enabled && vi_sense_get_voltage() < discharge_voltage) {

            load_set_enable(false);
        }

        kernel_sleep_ms(100);
    }
}

void load_set_current(uint16_t current_ma) {

    iset_dac_set_current(current_ma);

    cmd_write(CMD_ADDRESS_CC_LEVEL, current_ma);
}

void load_set_enable(bool state) {

    gpio_write(LOAD_EN_L_GPIO, state);
    gpio_write(LOAD_EN_R_GPIO, state);
    gpio_write(LED_GREEN_GPIO, !state);

    enabled = state;

    cmd_write(CMD_ADDRESS_STATUS, state);
}

void load_set_discharge_voltage(uint32_t voltage_mv) {

    discharge_voltage = voltage_mv;
}

void trigger_fault(load_fault_t fault) {

    load_set_enable(false);

    gpio_write(LED_RED_GPIO, LOW);
}