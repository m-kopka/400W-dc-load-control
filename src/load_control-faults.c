#include "load_control.h"
#include "cmd_spi_driver.h"

//---- INTERNAL DATA ---------------------------------------------------------------------------------------------------------------------------------------------

// load registers
extern uint16_t status_register;    // load status flags
extern uint16_t fault_register;     // load fault flags
extern uint16_t fault_mask;         // fault mask; if the corresponding bit in the fault mask is 0, the fault flag is ignored

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

void __check_fault_conditions(void);

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// sets the specified fault in the fault register and puts the load in a fault state if the fault is masked
void load_trigger_fault(load_fault_t fault) {

    uint16_t old_fault_register = fault_register;
    fault_register |= fault;
    if (old_fault_register == fault_register) return;   // fault already triggered, skip

    __check_fault_conditions();     // test fault status with fault mask and disable the load if the fault conditions are met
    
    cmd_write(CMD_ADDRESS_FAULT, fault_register);       // update the fault register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// clears a load fault flag
void load_clear_fault(load_fault_t fault) {

    fault_register &= ~fault;
    __check_fault_conditions();     // test fault status with fault mask and disable the load if the fault conditions are met

    cmd_write(CMD_ADDRESS_FAULT, fault_register);       // update the fault register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the load fault mask
void load_set_fault_mask(load_fault_t mask) {

    fault_mask = mask | LOAD_NON_MASKABLE_FAULTS;       // don't allow the always masked faults to be cleared
    __check_fault_conditions();                         // test fault status with fault mask and disable the load if the fault conditions are met

    cmd_write(CMD_ADDRESS_FAULT_MASK, fault_mask);       // update the fault mask register
}

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

// checks if the current state of the fault register and fault mask should cause a load fault state
// if yes, it disables the load and updates the fault state and status register
// called after fault register of fault mask is updated
void __check_fault_conditions(void) {

    // if any of the fault flags are not masked
    if (fault_register & fault_mask) {

        load_set_enable(false);

        // external fault doesn't turn on the FAULT LED and doesn't cause the module to pull down its FAULT pin
        if (fault_register & ~LOAD_FAULT_EXTERNAL) {

            gpio_write(EXT_FAULT_GPIO, LOW);
            gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_OUTPUT);
            gpio_write(FAULT_LED_GPIO, LOW);

        } else {    // only external fault is triggered

            gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_INPUT);
            gpio_write(FAULT_LED_GPIO, HIGH);
        }

        status_register |= LOAD_STATUS_FAULT;       // set the fault bit in the status register

    } else {    // unmasked faults are triggered

        gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_INPUT);
        gpio_write(FAULT_LED_GPIO, HIGH);

        status_register &= ~LOAD_STATUS_FAULT;   // clear the fault bit in the status register
    }
    
    cmd_write(CMD_ADDRESS_STATUS, status_register);     // update the status register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// debounces the external fault pin and triggers EXT faults
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
