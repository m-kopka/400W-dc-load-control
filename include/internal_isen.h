#ifndef _INTERNAL_ISEN_H_
#define _INTERNAL_ISEN_H_

/*
 *  this driver handles the internal ADC current sense for the individual current sinks
*/

#include "common_defs.h"

//---- ENUMERATIONS ----------------------------------------------------------------------------------------------------------------------------------------------

typedef enum {

    CURRENT_L1,
    CURRENT_L2,
    CURRENT_R1,
    CURRENT_R2

} internal_isen_t;

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the ADC for internal current sensing
void internal_isen_init(void);

// measures current of a individual current sink [mA]
uint16_t internal_isen_read(internal_isen_t current_sink);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _INTERNAL_ISEN_H_ */