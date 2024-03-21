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

void vi_sense_set_automatic_vsen_source(bool enable);

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the selected voltage sense source
static inline vsen_src_t vi_sense_get_vsen_source(void) {

    extern vsen_src_t vsen_src;
    return (vsen_src);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns current load voltage [mV]
static inline uint32_t vi_sense_get_voltage(void) {

    extern uint32_t load_voltage_mv;
    return (load_voltage_mv);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns current load current [mA]
static inline uint32_t vi_sense_get_current(void) {

    extern uint32_t load_current_ma;
    return (load_current_ma);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _VI_SENSE_H_ */