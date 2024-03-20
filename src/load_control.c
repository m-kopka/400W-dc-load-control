#include "load_control.h"
#include "iset_dac.h"
#include "vi_sense.h"
#include "cmd_interface/cmd_spi_driver.h"

uint16_t cc_level = 0;
static uint32_t discharge_voltage = 0;
bool enabled = false;
uint16_t fault_register = 0;
uint16_t fault_mask = 0xffff;

kernel_time_t last_enable_time = 0;

void ext_fault_task(void);

void __check_fault_conditions(void);

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the load and handles various load functions on runtime
void load_control_task(void) {

    // LEDs
    rcc_enable_peripheral_clock(LOAD_ENABLE_LED_GPIO_CLOCK);
    rcc_enable_peripheral_clock(FAULT_LED_GPIO_CLOCK);
    gpio_write(LOAD_ENABLE_LED_GPIO, HIGH);
    gpio_write(FAULT_LED_GPIO, HIGH);
    gpio_set_mode(LOAD_ENABLE_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_mode(FAULT_LED_GPIO, GPIO_MODE_OUTPUT);

    // Power board enable pins
    rcc_enable_peripheral_clock(LOAD_EN_L_GPIO_CLOCK);
    rcc_enable_peripheral_clock(LOAD_EN_R_GPIO_CLOCK);
    gpio_set_mode(LOAD_EN_L_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_mode(LOAD_EN_R_GPIO, GPIO_MODE_OUTPUT);
    
    iset_dac_init();

    uint32_t vi_sense_stack[64];
    uint32_t ext_fault_stack[64];
    kernel_create_task(vi_sense_task, vi_sense_stack, sizeof(vi_sense_stack), 10);
    kernel_create_task(ext_fault_task, ext_fault_stack, sizeof(ext_fault_stack), 10);

    kernel_sleep_ms(500);

    load_set_cc_level(LOAD_START_CC_LEVEL_MA);

    while (1) {

        if (enabled) {

            uint16_t enable_time_s = kernel_get_time_since(last_enable_time) / 1000;
            cmd_write(CMD_ADDRESS_ENA_TIME, enable_time_s);
        }

        if (enabled && vi_sense_get_voltage() < discharge_voltage) {

            load_set_enable(false);
        }

        if (enabled && vi_sense_get_voltage() * vi_sense_get_current() / 1000 > LOAD_MAX_POWER_MW) {

            load_trigger_fault(LOAD_FAULT_OPP);
        }

        kernel_sleep_ms(100);
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// enables or disables the load; returns true if the action was successful
bool load_set_enable(bool state) {

    if (state == enabled) return true;                          // load is already in the specified state
    if (state && (fault_register & fault_mask)) return false;   // load is in fault; enable not allowed until all masked faults are cleared

    if (state) {    // enable the load

        // set the minimum current level, enable the power boards and slowly increase the current to CC level
        iset_dac_set_current(LOAD_MIN_CURRENT_MA, false);
        gpio_write(LOAD_EN_L_GPIO, HIGH);
        gpio_write(LOAD_EN_R_GPIO, HIGH);
        iset_dac_set_current(cc_level, true);
        gpio_write(LOAD_ENABLE_LED_GPIO, LOW);

        last_enable_time = kernel_get_time_ms();

    } else {    // disable the load

        // disable the power boards and write zero-current code to I_SET DAC
        gpio_write(LOAD_EN_L_GPIO, LOW);
        gpio_write(LOAD_EN_R_GPIO, LOW);
        iset_dac_write_code(0xffff);
        gpio_write(LOAD_ENABLE_LED_GPIO, HIGH);
    }

    enabled = state;
    cmd_write(CMD_ADDRESS_STATUS, state);       // update the status register

    return true;
}

bool load_get_enable(void) {

    return enabled;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the load current in Constant Current mode
void load_set_cc_level(uint16_t current_ma) {

    // check limits
    if (current_ma < LOAD_MIN_CURRENT_MA) current_ma = LOAD_MIN_CURRENT_MA;
    if (current_ma > LOAD_MAX_CURRENT_MA) current_ma = LOAD_MAX_CURRENT_MA;

    if (enabled) iset_dac_set_current(current_ma, true);    // if the load is enabled write new value to the I_SET dac with a slew rate limit

    cc_level = current_ma;
    cmd_write(CMD_ADDRESS_CC_LEVEL, current_ma);        // update the register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void load_set_discharge_voltage(uint32_t voltage_mv) {

    discharge_voltage = voltage_mv;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void load_trigger_fault(load_fault_t fault) {

    fault_register |= fault;
    __check_fault_conditions();     // test fault status with fault mask and disable the load if the fault conditions are met
    
    cmd_write(CMD_ADDRESS_FAULT1, fault_register);
}

load_fault_t load_get_faults(load_fault_t mask) {

    return (fault_register & mask);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void load_set_fault_mask(load_fault_t mask) {

    fault_mask = mask | LOAD_UNMASKABLE_FAULTS;
    __check_fault_conditions();     // test fault status with fault mask and disable the load if the fault conditions are met
}

uint16_t load_get_fault_mask(void) {

    return fault_mask;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void load_clear_fault(load_fault_t fault) {

    fault_register &= ~fault;

    // fault condition no longer met, external fault is ignored as the fault led isnt on during ext fault
    if (!((fault_register & ~LOAD_FAULT_EXTERNAL) & fault_mask)) {

        gpio_write(FAULT_LED_GPIO, HIGH);
        gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_INPUT);
    }

    cmd_write(CMD_ADDRESS_FAULT1, fault_register);
}

void __check_fault_conditions(void) {

    if (fault_register & fault_mask) {

        load_set_enable(false);

        if (fault_register & ~LOAD_FAULT_EXTERNAL) {

            gpio_write(EXT_FAULT_GPIO, LOW);
            gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_OUTPUT);
            
            gpio_write(FAULT_LED_GPIO, LOW);        // external fault doesnt turn on the LED
        }
    }
}

void ext_fault_task(void) {

    rcc_enable_peripheral_clock(EXT_FAULT_GPIO_CLOCK);
    gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_INPUT);

    while (1) {

        static uint8_t debounce_counter = 0xff;
        static bool triggered = false;

        debounce_counter = (debounce_counter << 1) | gpio_get(EXT_FAULT_GPIO);

        if (debounce_counter == 0x00) {

            if (!triggered && !(fault_register & fault_mask)) {

                load_trigger_fault(LOAD_FAULT_EXTERNAL);
                triggered = true;
            }

        } else if (debounce_counter == 0xff) triggered = false;

        kernel_yield();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
