#ifndef _CMD_SPI_REGISTERS_H_
#define _CMD_SPI_REGISTERS_H_

/*
 *  LOAD CMD SPI specifications and address map
 *  Martin Kopka 2024
*/

#include "common_defs.h"

//---- DATA FRAME SPECIFICATIONS ---------------------------------------------------------------------------------------------------------------------------------

#define CMD_FRAME_SYNC_BYTE    0xff         // first byte of each frame; slave starts receiving a frame after FRAME_SYNC_BYTE is received
#define CMD_READ_BIT           0x80000000   // if R/!W bit is 0 => write command, if 1 => read command; bit 31 od a frame (frame sync byte excluded) bit 7 of address byte

//---- REGISTER MAP ----------------------------------------------------------------------------------------------------------------------------------------------

typedef enum {

    CMD_ADDRESS_ID          = 0x00,
    CMD_ADDRESS_STATUS      = 0x01,
    CMD_ADDRESS_CONFIG      = 0x02,
    CMD_ADDRESS_WD_RELOAD   = 0x04,
    CMD_ADDRESS_FAULT       = 0x08,
    CMD_ADDRESS_FAULTMSK    = 0x10,
    CMD_ADDRESS_ENABLE      = 0x14,
    CMD_ADDRESS_CC_LEVEL    = 0x15,
    CMD_ADDRESS_VIN         = 0x19, // HI LO
    CMD_ADDRESS_ITOT        = 0x20,
    CMD_ADDRESS_IL1         = 0x21,
    CMD_ADDRESS_IL2         = 0x22,
    CMD_ADDRESS_IR1         = 0x23,
    CMD_ADDRESS_IR2         = 0x24,
    CMD_ADDRESS_TEMP_L      = 0x30,
    CMD_ADDRESS_TEMP_R      = 0x31,
    CMD_ADDRESS_RPM1        = 0x38,
    CMD_ADDRESS_RPM2        = 0x39,
    CMD_ADDRESS_ENA_TIME    = 0x40,  // HI LO
    CMD_ADDRESS_TOTAL_MAH   = 0x42,
    CMD_ADDRESS_TOTAL_MWH   = 0x43      //HI LO

} cmd_register_t;

#define CMD_REGISTER_COUNT ((CMD_ADDRESS_TOTAL_MWH) + 1)

#define cmd_address_valid(address) (((address) < CMD_REGISTER_COUNT))

//---- REGISTER DESCRIPTION --------------------------------------------------------------------------------------------------------------------------------------

// slave always responds to CMD_ID read request with CMD_ID_CODE; writing to CMD_ID has no effect
#define CMD_ID_CODE    0x10AD

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// status register bits
typedef enum {

    LOAD_STATUS_ENABLED  = (1 << 0),
    LOAD_STATUS_FAULT    = (1 << 1),
    LOAD_STATUS_READY    = (1 << 2)

} load_status_t;

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

typedef enum {

    LOAD_FAULT_COMMUNICATION = 0x0001,
    LOAD_FAULT_CHECKSUM      = 0x0002,
    LOAD_FAULT_OTP           = 0x0004,
    LOAD_FAULT_TEMP_SENSOR_L = 0x0008,
    LOAD_FAULT_TEMP_SENSOR_R = 0x0010,
    LOAD_FAULT_FAN1          = 0x0020,
    LOAD_FAULT_FAN2          = 0x0040,
    LOAD_FAULT_OCP           = 0x0080,
    LOAD_FAULT_FUSE_L1       = 0x0100,
    LOAD_FAULT_FUSE_L2       = 0x0200,
    LOAD_FAULT_FUSE_R1       = 0x0400,
    LOAD_FAULT_FUSE_R2       = 0x0800,
    LOAD_FAULT_EXTERNAL      = 0x1000,
    LOAD_FAULT_OPP           = 0x2000,
    LOAD_FAULT_ALL           = 0x2fff

} load_fault_t;

// fault flags which are for safety reasons not allowed to be unmasked
#define LOAD_ALWAYS_MASKED_FAULTS (LOAD_FAULT_OTP | LOAD_FAULT_TEMP_SENSOR_L | LOAD_FAULT_TEMP_SENSOR_R)

// fault mask on startup
#define LOAD_DEFAULT_FAULT_MASK   (LOAD_FAULT_ALL & ~(LOAD_FAULT_COMMUNICATION | LOAD_FAULT_EXTERNAL))

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

#define LOAD_WD_RELOAD_KEY 0xBABA

#define LOAD_ENABLE_KEY 0xABCD

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _CMD_SPI_REGISTERS_H_ */
