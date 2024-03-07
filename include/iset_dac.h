#ifndef _ISET_DAC_H_
#define _ISET_DAC_H_

/*
 *  Device driver for the LTC2641-16 DAC
 *  Martin Kopka 2024
 *
 *  The driver communicates with the DAC via SPI interface in order to set the desired load current
 *  DAC resolution: 16bits
 *  DAC settling time 1us
 *  DAC maximum SPI frequency: 50MHz
 *  https://cz.mouser.com/datasheet/2/609/26412fd-3125252.pdf
 */

#include "common_defs.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the SPI communication with the I_SET DAC and sets it to the 0A current level
void iset_dac_init(void);

// sets the I_SET DAC output voltage to the corresponding current value; returns the real set current in mA clamped and rounded by the driver
uint32_t iset_dac_set_current(uint32_t current_ma);

// sets the specified code to the ISET_DAC (for calibration)
void iset_dac_set_raw(uint16_t code);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _ISET_DAC_H_ */