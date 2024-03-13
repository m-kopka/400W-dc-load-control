#include "debug_uart.h"
#include "hal/uart.h"

//---- CONSTANTS -------------------------------------------------------------------------------------------------------------------------------------------------

#define MAX_BYTES_PARSED    64      // maximum parsed bytes per update to avoid missed deadlines of other tasks or infinite loop in case of a fault

//---- PRIVATE FUNCTIONS -----------------------------------------------------------------------------------------------------------------------------------------

void shell_print_header(void);
void shell_update(char *buffer);

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// handles responding to commands from the DEBUG_UART
void debug_uart_task(void) {

    char tx_fifo[512];              // transmit fifo to be provided to the UART driver
    char rx_fifo[128];              // receive fifo to be provided to the UART driver
    char message_buffer[128];       // buffer for parsing an incomming message until a newline is received
    uint32_t buffer_pos = 0;        // current index into the message_buffer

    uart_init(DEBUG_UART, DEBUG_UART_BAUD, DEBUG_UART_TX_GPIO, DEBUG_UART_RX_GPIO, (char*)&tx_fifo, sizeof(tx_fifo), (char*)&rx_fifo, sizeof(rx_fifo));
    shell_print_header();

    while (1) {

        uint32_t bytes_parsed = 0;          // limit parsed bytes per update to avoid missed deadlines of other tasks or infinite loop in case of a fault

        while (uart_has_data(DEBUG_UART) && bytes_parsed < MAX_BYTES_PARSED) {

            char c = uart_getc(DEBUG_UART);
            bytes_parsed++;

            // if newline reached or the buffer was filled, handle incomming message by sending it to the shell
            if (c == '\n' || buffer_pos == sizeof(message_buffer)) {

                message_buffer[buffer_pos] = '\0';       // append a null-terminator at the end
                buffer_pos = 0;

                shell_update(message_buffer);

            } else message_buffer[buffer_pos++] = c;     // else continue parsing
        }

        kernel_yield();
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// transmits a null-terminated string via DEBUG_UART
void debug_print(const char *str) {

    uart_puts(DEBUG_UART, str);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// converts a number to string and sends it via DEBUG_UART
void debug_print_int(int num) {

    uart_puti(DEBUG_UART, num);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// divides a number by 1000, converts it to string and sends it via DEBUG_UART with 2 decimal places
void debug_print_int_dec(int num) {

    char temp_buff[16];
    itoa(num, temp_buff, 16, 10);

    uint8_t total_places = strlen(temp_buff);
    int i;

    if (num < 1000) uart_putc(DEBUG_UART, '0');

    for (i = 0; i < total_places - 3; i++) uart_putc(DEBUG_UART, temp_buff[i]);     // print integral part

    uart_putc(DEBUG_UART, '.');     // decimal place

    if (num < 100) uart_putc(DEBUG_UART, '0');

    uart_putc(DEBUG_UART, temp_buff[i++]);  // print first decimal place

    // if the value is 0, temp_buff[1] is not populated
    if (num > 0) {

        // round last digit to nearest value
        if (temp_buff[i] < '5') uart_putc(DEBUG_UART, temp_buff[i++]);
        else uart_putc(DEBUG_UART, temp_buff[i++] + 1);

    } else uart_putc(DEBUG_UART, '0');
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// converts a number to string in a hexadecimal format and sends it via DEBUG_UART
void debug_print_int_hex(int num, uint8_t hex_digits) {

    char temp_buff[16];
    itoa(num, temp_buff, 16, 16);

    uint32_t digits = strlen(temp_buff);
    for (int i = 0; i < hex_digits - digits; i++) debug_print("0");

    debug_print(temp_buff);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
