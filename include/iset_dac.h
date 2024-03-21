#ifndef _ISET_DAC_H_
#define _ISET_DAC_H_

/*
 *  Device driver for the LTC2641-16 DAC
 *  Martin Kopka 2024
 *
 *  The driver communicates with the DAC via SPI interface in order to set the required load current
 *  DAC resolution: 16bits
 *  DAC settling time 1us
 *  DAC maximum SPI frequency: 50MHz
 *  https://cz.mouser.com/datasheet/2/609/26412fd-3125252.pdf
 */

#include "common_defs.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the SPI communication with the I_SET DAC and sets it to the 0A current level
void iset_dac_init(void);

// sets the I_SET DAC output voltage to the corresponding current value
void iset_dac_set_current(uint32_t current_ma, bool slew_limit);

// writes the specified 16bit code to the ISET_DAC
void iset_dac_write_code(uint16_t code);

bool iset_dac_is_in_transient(void);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _ISET_DAC_H_ */