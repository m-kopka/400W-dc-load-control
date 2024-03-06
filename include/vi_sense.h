#ifndef _VI_SENSE_H_
#define _VI_SENSE_H_

#include "common_defs.h"

typedef enum {

    VSEN_SRC_INTERNAL = 0x0,
    VSEN_SRC_REMOTE = 0x1

} vsen_src_t;

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

void vi_sense_task(void);

uint32_t vi_sense_get_voltage(void);

uint32_t vi_sense_get_current(void);

void vi_sense_set_vsen_source(vsen_src_t source);

vsen_src_t vi_sense_get_vsen_source(void);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _EXT_ADC_H_ */