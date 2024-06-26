#ifndef _VI_SENSE_H_
#define _VI_SENSE_H_

/*
 *  Device driver for the AD7091R ADC
 *  Martin Kopka 2024
 *
 *  The driver communicates with 2 instances of the AD7091R ADC to sample load voltage and current
 *  ADC resolution: 12bits
 *  ADC maximum SPI frequency: 50MHz
 *  https://cz.mouser.com/datasheet/2/609/AD7091R-3118521.pdf
 */

#include "common_defs.h"
#include "internal_isen.h"

//---- ENUMERATIONS ----------------------------------------------------------------------------------------------------------------------------------------------

typedef enum {

    VSEN_SRC_INTERNAL = 0x0,
    VSEN_SRC_REMOTE = 0x1

} vsen_src_t;

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// samples load voltage and current and rounds the results
void vi_sense_task(void);

// sets the VSEN ADC source (either VSEN_SRC_INTERNAL or VSEN_SRC_REMOTE)
void vi_sense_set_vsen_source(vsen_src_t source);

// enables or disables the automatic VSEN source switching feature
void vi_sense_set_automatic_vsen_source(bool enable);

// enables or disables the Continuous Conversion Mode (new conversion trigger and read is triggered immediately after reading the last one)
void vi_sense_set_continuous_conversion_mode(bool enabled);

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the selected voltage sense source
static inline vsen_src_t vi_sense_get_vsen_source(void) {

    extern vsen_src_t vsen_src;
    return (vsen_src);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns current load voltage [mV]
static inline uint32_t vi_sense_get_voltage(void) {

    extern int32_t load_voltage_mv;
    return (load_voltage_mv);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns current load current [mA]
static inline uint32_t vi_sense_get_current(void) {

    extern int32_t load_current_ma;
    return (load_current_ma);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns current load power [mW]
static inline uint32_t vi_sense_get_power(void) {

    extern int32_t load_power_mw;
    return (load_power_mw);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the current of a individual current sink [mA]
static inline uint32_t vi_sense_get_sink_current(internal_isen_t sink) {

    if (sink > CURRENT_R2) return 0;

    extern uint32_t sink_current[4];
    return sink_current[sink];
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _VI_SENSE_H_ */