#ifndef _CMD_SPI_REGISTERS_H_
#define _CMD_SPI_REGISTERS_H_

#include "common_defs.h"

//---- DATA FRAME SPECIFICATIONS ---------------------------------------------------------------------------------------------------------------------------------

#define CMD_FRAME_SYNC_BYTE    0xff
#define CMD_READ_BIT           0x80000000

//---- REGISTER MAP ----------------------------------------------------------------------------------------------------------------------------------------------

typedef enum {

    CMD_ADDRESS_ID          = 0x00,
    CMD_ADDRESS_STATUS      = 0x01,
    CMD_ADDRESS_CONFIG1     = 0x02,
    CMD_ADDRESS_CONFIG2     = 0x03,
    CMD_ADDRESS_FAULT1      = 0x06,
    CMD_ADDRESS_FAULT2      = 0x07,
    CMD_ADDRESS_FAULTMSK1   = 0x10,
    CMD_ADDRESS_ENABLE      = 0x14,
    CMD_ADDRESS_CC_LEVEL    = 0x15,
    CMD_ADDRESS_VIN         = 0x19,
    CMD_ADDRESS_ITOT        = 0x20,
    CMD_ADDRESS_IL1         = 0x21,
    CMD_ADDRESS_IL2         = 0x22,
    CMD_ADDRESS_IR1         = 0x23,
    CMD_ADDRESS_IR2         = 0x24,
    CMD_ADDRESS_TEMP_L      = 0x30,
    CMD_ADDRESS_TEMP_R      = 0x31,
    CMD_ADDRESS_RPM1        = 0x38,
    CMD_ADDRESS_RPM2        = 0x39,

} cmd_register_t;

#define CMD_REGISTER_COUNT ((CMD_ADDRESS_RPM2) + 1)

#define cmd_address_valid(address) (((address) < CMD_REGISTER_COUNT))

typedef enum {

    LOAD_FAULT_COMMUNICATION = 0x0001,
    LOAD_FAULT_CHECKSUM      = 0x0002,
    LOAD_FAULT_OTP           = 0x0004,
    LOAD_FAULT_TEMP_SENSOR   = 0x0008,
    LOAD_FAULT_FAN           = 0x000c,

} load_fault_t;

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _CMD_SPI_REGISTERS_H_ */
