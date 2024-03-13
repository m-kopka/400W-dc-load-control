#ifndef _LOAD_CONTROL_H_
#define _LOAD_CONTROL_H_

#include "common_defs.h"
#include "cmd_interface/cmd_spi_driver.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

void load_control_task(void);

void load_set_current(uint16_t current_ma);

void load_set_enable(bool state);

void trigger_fault(load_fault_t fault);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _LOAD_CONTROL_H_ */
