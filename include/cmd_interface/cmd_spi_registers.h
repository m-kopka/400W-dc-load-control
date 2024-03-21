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

    CMD_ADDRESS_ID              = 0x00,     // Load ID register (r), always returns 0x10AD
    CMD_ADDRESS_STATUS          = 0x01,     // Load Status register (r)
    CMD_ADDRESS_CONFIG          = 0x02,     // Load Configuration Register (r/w)
    CMD_ADDRESS_FAULT           = 0x04,     // Load Fault Flag register (r)
    CMD_ADDRESS_FAULT_MASK      = 0x08,     // Load Fault Mask register (r/w)
    CMD_ADDRESS_WD_RELOAD       = 0x0C,     // Load Watchdog Reload register (w), write 0xBABA to reload the watchdog
    CMD_ADDRESS_ENABLE          = 0x0D,     // Load Enable register (w), write 0xABCD to enable the load, write 0 to disable
    CMD_ADDRESS_CC_LEVEL        = 0x10,     // Load CC Level register (r/w)
    CMD_ADDRESS_DISCH_LEVEL     = 0x14,     // Load Discharge Voltage register (r/w)
    CMD_ADDRESS_AVLBL_CURRENT   = 0x1E,     // Load Available Current register (r)
    CMD_ADDRESS_AVLBL_POWER     = 0x1F,     // Load Available Power register (r)
    CMD_ADDRESS_VOLTAGE         = 0x20,     // Load Input Voltage register (r)
    CMD_ADDRESS_CURRENT         = 0x22,     // Load Total Current register (r)
    CMD_ADDRESS_CURRENT_L1      = 0x28,     // Load L1 Sink Current register (r)
    CMD_ADDRESS_CURRENT_L2      = 0x29,     // Load L2 Sink Current register (r)
    CMD_ADDRESS_CURRENT_R1      = 0x2A,     // Load R1 Sink Current register (r)
    CMD_ADDRESS_CURRENT_R2      = 0x2B,     // Load R2 Sink Current register (r)
    CMD_ADDRESS_TEMP_L          = 0x30,     // Left Power Board Temperature register (r)
    CMD_ADDRESS_TEMP_R          = 0x31,     // Right Power Board Temperature register (r)
    CMD_ADDRESS_FAN_RPM1        = 0x38,     // FAN1 RPM register (r)
    CMD_ADDRESS_FAN_RPM2        = 0x39,     // FAN2 RPM register (r)
    CMD_ADDRESS_TOTAL_TIME_L    = 0x40,     // Load Total Running Time low register (r)
    CMD_ADDRESS_TOTAL_TIME_H    = 0x41,     // Load Total Running Time high register (r)
    CMD_ADDRESS_TOTAL_MAH       = 0x42,     // Load Total Milliamphours register (r)
    CMD_ADDRESS_TOTAL_MWH_L     = 0x44,     // Load Total Milliwatthours low register (r)
    CMD_ADDRESS_TOTAL_MWH_H     = 0x45,     // Load Total Milliwatthours high register (r)

} cmd_register_t;

#define CMD_REGISTER_COUNT ((CMD_ADDRESS_TOTAL_MWH_H) + 1)

// returns true if the specified address is in the load's register space
#define cmd_address_valid(address) (((address) < CMD_REGISTER_COUNT))

//---- REGISTER DESCRIPTION --------------------------------------------------------------------------------------------------------------------------------------

// slave always responds to CMD_ID read request with LOAD_ID_CODE; writing to CMD_ID has no effect
#define LOAD_ID_CODE    0x10AD

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// status register bits
typedef enum {

    LOAD_STATUS_ENABLED  = (1 << 0),
    LOAD_STATUS_FAULT    = (1 << 1),
    LOAD_STATUS_READY    = (1 << 2)

} load_status_t;

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// config register bits
typedef enum {

    LOAD_CONFIG_VSEN_SRC        = (1 <<  4),    // 0 -> internal, 1 -> remote (this bit is ignored in write commands if the AUTO_VSEN_SRC bit is set)
    LOAD_CONFIG_STAT_RESET      = (1 << 15)     // write 1 to reset the load stats (total time, mAh and mWh)
    
} load_config_t;

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

typedef enum {

    LOAD_FAULT_COM      = (1 <<  0),
    LOAD_FAULT_OTP      = (1 <<  1),
    LOAD_FAULT_OCP      = (1 <<  2),
    LOAD_FAULT_OPP      = (1 <<  3),
    LOAD_FAULT_TEMP_L   = (1 <<  4),
    LOAD_FAULT_TEMP_R   = (1 <<  5),
    LOAD_FAULT_FAN1     = (1 <<  6),
    LOAD_FAULT_FAN2     = (1 <<  7),
    LOAD_FAULT_FUSE_L1  = (1 <<  8),
    LOAD_FAULT_FUSE_L2  = (1 <<  9),
    LOAD_FAULT_FUSE_R1  = (1 << 10),
    LOAD_FAULT_FUSE_R2  = (1 << 11),
    LOAD_FAULT_EXTERNAL = (1 << 12),
    
    LOAD_FAULT_ALL      = 0x1FFF

} load_fault_t;

// fault flags which are for safety reasons not allowed to be masked
#define LOAD_NON_MASKABLE_FAULTS (LOAD_FAULT_OTP | LOAD_FAULT_OPP | LOAD_FAULT_TEMP_L | LOAD_FAULT_TEMP_R)

// fault mask on startup
#define LOAD_DEFAULT_FAULT_MASK   (LOAD_FAULT_ALL & ~(LOAD_FAULT_COM | LOAD_FAULT_EXTERNAL))

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// writing the reload key to the WD_RELOAD register reloads the watchdog and prevents triggering the COM fault
#define LOAD_WD_RELOAD_KEY  0xBABA

// if the load is enabled, the master needs to write the reload key to the WD_RELOAD register
// if the watchdog timeout is reached the COM fault is triggered
#define LOAD_WD_TIMEOUT_MS  1000

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// writing the enable key to the ENABLE register enables the load
#define LOAD_ENABLE_KEY 0xABCD

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _CMD_SPI_REGISTERS_H_ */
