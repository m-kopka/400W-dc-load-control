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

// measures a power transistor temperature and returns it [°C]. Returns 255 in case of a fault
uint8_t temp_sensor_read(temp_sensor_t sensor);

// measures a power transistor temperature and returns it [°C] in UQ8.2 fixed point value. Returns 1023 in case of a fault
uint16_t temp_sensor_read_fixed(temp_sensor_t sensor);

// returns the raw 12bit ADC reading of a power transistor temperature (for calibration)
uint16_t temp_sensor_read_raw(temp_sensor_t sensor);

// returns the temperature sensor's fault flags
temp_sensor_fault_t temp_sensor_read_faults(temp_sensor_t sensor);

// converts a temperature value from an integer to UQ8.2 fixed point
static inline uint16_t temp_sensor_int_to_q8_2(uint8_t temp_c) {return (temp_c << 2);}

// converts a temperature value from UQ8.2 fixed point to an integer
static inline uint8_t temp_sensor_q8_2_to_int(uint16_t temp_q8_2) {return (temp_q8_2 >> 2);}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _TEMP_SENSORS_H_ */
