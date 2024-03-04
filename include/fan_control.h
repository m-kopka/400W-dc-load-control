#ifndef _FAN_CONTROL_H_
#define _FAN_CONTROL_H_

#include "common_defs.h"
#include "hal/timer.h"

typedef enum {

    FAN1 = 0,
    FAN2 = 1,

} fan_t;

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes hardware for fan PWM control and speed measurement. Then continuously controls the fan speed based on the set point
void fan_regulator_task(void);

// sets the target fan speed for both fans
void fan_set_speed(uint8_t speed);

// sets the target fan speed override for both fans
// this is used to control the fan speed externally for example by a user; the override cannot set a lower speed than the speed set by the temperature control logic
void fan_set_speed_override(uint8_t speed);

// returns the speed of FAN1 or FAN2 in RPM
uint16_t fan_get_rpm(fan_t fan_num);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _FAN_CONTROL_H_ */
