#include "load_control.h"
#include "iset_dac.h"
#include "internal_isen.h"
#include "vi_sense.h"
#include "cmd_interface/cmd_spi_driver.h"

uint16_t cc_level = 0;
static uint32_t discharge_voltage = 0;
bool enabled = false;
uint16_t fault_register = 0;

void ext_fault_task(void);

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

void load_control_task(void) {

    // LEDs
    rcc_enable_peripheral_clock(LED_GREEN_GPIO_CLOCK);
    rcc_enable_peripheral_clock(LED_RED_GPIO_CLOCK);
    gpio_write(LED_GREEN_GPIO, HIGH);
    gpio_write(LED_RED_GPIO, HIGH);
    gpio_set_mode(LED_GREEN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_mode(LED_RED_GPIO, GPIO_MODE_OUTPUT);

    // Power board enable pins
    rcc_enable_peripheral_clock(LOAD_EN_L_GPIO_CLOCK);
    rcc_enable_peripheral_clock(LOAD_EN_R_GPIO_CLOCK);
    gpio_set_mode(LOAD_EN_L_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_mode(LOAD_EN_R_GPIO, GPIO_MODE_OUTPUT);
    
    iset_dac_init();
    internal_isen_init();

    uint32_t vi_sense_stack[64];
    uint32_t ext_fault_stack[64];
    kernel_create_task(vi_sense_task, vi_sense_stack, sizeof(vi_sense_stack), 100);
    kernel_create_task(ext_fault_task, ext_fault_stack, sizeof(ext_fault_stack), 10);

    load_set_cc_level(LOAD_START_CC_LEVEL_MA);

    while (1) {

        if (enabled && vi_sense_get_voltage() < discharge_voltage) {

            load_set_enable(false);
        }

        kernel_sleep_ms(100);
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void load_set_enable(bool state) {

    if (state == enabled) return;
    if (state && fault_register) return;

    if (state) {

        iset_dac_set_current(LOAD_MIN_CURRENT_MA, false);
        gpio_write(LOAD_EN_L_GPIO, HIGH);
        gpio_write(LOAD_EN_R_GPIO, HIGH);
        iset_dac_set_current(cc_level, true);
        gpio_write(LED_GREEN_GPIO, LOW);

    } else {

        gpio_write(LOAD_EN_L_GPIO, LOW);
        gpio_write(LOAD_EN_R_GPIO, LOW);
        iset_dac_write_code(0xffff);
        gpio_write(LED_GREEN_GPIO, HIGH);
    }

    enabled = state;
    cmd_write(CMD_ADDRESS_STATUS, state);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void load_set_cc_level(uint16_t current_ma) {

    if (current_ma < LOAD_MIN_CURRENT_MA) current_ma = LOAD_MIN_CURRENT_MA;
    if (current_ma > LOAD_MAX_CURRENT_MA) current_ma = LOAD_MAX_CURRENT_MA;

    if (enabled) iset_dac_set_current(current_ma, true);

    cc_level = current_ma;
    cmd_write(CMD_ADDRESS_CC_LEVEL, current_ma);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void load_set_discharge_voltage(uint32_t voltage_mv) {

    discharge_voltage = voltage_mv;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void load_trigger_fault(load_fault_t fault) {

    load_set_enable(false);
    gpio_write(LED_RED_GPIO, LOW);

    fault_register |= fault;
    cmd_write(CMD_ADDRESS_FAULT1, fault_register);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void clear_fault() {

    gpio_write(LED_RED_GPIO, HIGH);

    fault_register = 0;
    cmd_write(CMD_ADDRESS_FAULT1, fault_register);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
