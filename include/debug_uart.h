#ifndef _DEBUG_UART_H_
#define _DEBUG_UART_H_

#include "common_defs.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// handles responding to commands from the DEBUG_UART
void debug_uart_task(void);

// transmits a null-terminated string via DEBUG_UART
void debug_print(const char *str);

// converts a number to string and sends it via DEBUG_UART
void debug_print_int(int num);

// divides a number by 1000, converts it to string and sends it via DEBUG_UART with 2 decimal places
void debug_print_int_dec(int num);

// converts a number to string in a hexadecimal format and sends it via DEBUG_UART
void debug_print_int_hex(int num, uint8_t hex_digits);

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _DEBUG_UART_H_ */