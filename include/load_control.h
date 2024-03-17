#ifndef _LOAD_CONTROL_H_
#define _LOAD_CONTROL_H_

#include "common_defs.h"
#include "cmd_interface/cmd_spi_driver.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

void load_control_task(void);

void load_set_enable(bool state);

void load_set_cc_level(uint16_t current_ma);

void load_set_discharge_voltage(uint32_t voltage_mv);

void load_trigger_fault(load_fault_t fault);

void load_set_fault_mask(load_fault_t mask);

void clear_fault();

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _LOAD_CONTROL_H_ */
