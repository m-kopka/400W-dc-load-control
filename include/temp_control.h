#ifndef _TEMPER_CONTROL_H_
#define _TEMPER_CONTROL_H_

#include "common_defs.h"
#include "fan_control.h"
#include "temp_sensors.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// regulates fan speed based on power transistor temperature
void temp_control_task(void);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _TEMP_CONTROL_H_ */
