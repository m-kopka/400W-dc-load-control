#ifndef _LOAD_CMD_H_
#define _LOAD_CMD_H_

#include "common_defs.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// handles write commands from master and communication watchdog timeout
void load_cmd_task(void);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _LOAD_CMD_H_ */
