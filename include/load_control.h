#ifndef _LOAD_CONTROL_H_
#define _LOAD_CONTROL_H_

#include "common_defs.h"
#include "cmd_spi_driver.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the load and handles various load functions on runtime
void load_control_task(void);

// enables or disables the load; returns true if the action was successful; returns false if the load is in a fault state or not ready
bool load_set_enable(bool state);

// sets the load mode (CC, CV, CR or CP)
void load_set_mode(load_mode_t mode);

// sets the load current in Constant Current mode
void load_set_cc_level(uint32_t current_ma);

// sets the load voltage in Constant Voltage mode
void load_set_cv_level(uint32_t voltage_mv);

// sets the load resistance in Constant Resistance mode
void load_set_cr_level(uint32_t resistance_mohm);

// sets the load power in Constant Power mode
void load_set_cp_level(uint32_t power_mw);

// sets the discharge threshold voltage; if the load voltage drops bellow this value, the load is automatically disabled
void load_set_discharge_voltage(uint32_t voltage_mv);

// sets the specified fault in the fault register and puts the load in a fault state if the fault is masked
void load_trigger_fault(load_fault_t fault);

// clears a load fault flag
void load_clear_fault(load_fault_t fault);

// sets the load fault mask
void load_set_fault_mask(load_fault_t mask);

// sets the ready flag in the status register
void load_set_ready(bool ready);

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the load status register
static inline load_mode_t load_get_mode(void) {

    extern load_mode_t load_mode;
    return load_mode;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the load status register
static inline uint16_t load_get_status(void) {

    extern uint16_t status_register;
    return status_register;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the state of the fault register
static inline uint16_t load_get_faults(load_fault_t mask) {

    extern uint16_t fault_register;
    extern uint16_t fault_mask;
    return (fault_register & mask);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the load fault mask
static inline uint16_t load_get_fault_mask(void) {

    extern uint16_t fault_mask;
    return fault_mask;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _LOAD_CONTROL_H_ */
