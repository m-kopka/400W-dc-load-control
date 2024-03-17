#ifndef _LOAD_CONTROL_H_
#define _LOAD_CONTROL_H_

#include "common_defs.h"
#include "cmd_interface/cmd_spi_driver.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the load and handles various load functions on runtime
void load_control_task(void);

// enables or disables the load; returns true if the action was successful
bool load_set_enable(bool state);

// sets the load current in Constant Current mode
void load_set_cc_level(uint16_t current_ma);

void load_set_discharge_voltage(uint32_t voltage_mv);

void load_trigger_fault(load_fault_t fault);

load_fault_t load_get_faults(load_fault_t mask);

void load_set_fault_mask(load_fault_t mask);

uint16_t load_get_fault_mask(void);

void load_clear_fault(load_fault_t fault);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _LOAD_CONTROL_H_ */
