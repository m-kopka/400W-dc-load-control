#ifndef _TEMP_SENSORS_H_
#define _TEMP_SENSORS_H_

/*
 *  This driver handles power trasistor temperature sensing
 *  the driver also detects if the sensor is disconnected or shorted
*/

#include "common_defs.h"
#include "fan_control.h"
#include "hal/adc.h"

//---- ENUMERATIONS ----------------------------------------------------------------------------------------------------------------------------------------------

typedef enum {

    TEMP_L = 0,
    TEMP_R = 1

} temp_sensor_t;

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// temperature sensor fault flags
typedef enum {

    TEMP_SEN_FAULT_NONE = 0x0,
    TEMP_SEN_FAULT_OPEN = 0x1,
    TEMP_SEN_FAULT_SHORT = 0x2,

} temp_sensor_fault_t;

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the ADC for power transistor temperature sensing
void temp_sensor_init(void);

// measures a power transistor temperature and returns it; returns 255 in case of a fault
uint8_t temp_sensor_read(temp_sensor_t sensor);

// returns the temperature sensor's fault flags
temp_sensor_fault_t temp_sensor_read_faults(temp_sensor_t sensor);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _TEMP_SENSORS_H_ */
