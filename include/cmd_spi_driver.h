#ifndef _CMD_SPI_DRIVER_H_
#define _CMD_SPI_DRIVER_H_

/*
 *  Load CMD SPI interface slave driver
 *  Martin Kopka 2024
 * 
 *  This module handles communication with the master (interface panel) via SPI
 *  write commands from the master are parsed into a RX fifo to be read asynchronously by the cmd_spi_task
 *  slave updates its values provided to the master by writing to its virtual registers
 *  values of these registers are then sent to the master as a response to a master read command
 */

#include "common_defs.h"
#include "cmd_spi_registers.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the CMD interface slave SPI driver
void cmd_driver_init(void);

// return true if there are unread write commands available in the RX buffer
bool cmd_has_data(void);

// reads one message from the write command buffer; callee should read the cmd_has_data() flag first; returns true if the checksum of the message is correct
bool cmd_read(uint8_t *address, uint16_t *data);

// writes data to the specified register; if the interface receives a read command on this address, this value will be transmitted to the master
void cmd_write(uint8_t address, uint16_t data);

// sets a bit in a load register
void cmd_set_bit(uint8_t address, uint16_t mask);

// clears a bit in a load register
void cmd_clear_bit(uint8_t address, uint16_t mask);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _CMD_SPI_DRIVER_H_ */
