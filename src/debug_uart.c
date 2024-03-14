#include "debug_uart.h"
#include "hal/uart.h"

//---- CONSTANTS -------------------------------------------------------------------------------------------------------------------------------------------------

#define MAX_BYTES_PARSED    64      // maximum parsed bytes per update to avoid missed deadlines of other tasks or infinite loop in case of a fault

//---- PRIVATE FUNCTIONS -----------------------------------------------------------------------------------------------------------------------------------------

void shell_print_header(void);
void shell_update(char *buffer);

//---- INTERNAL DATA ---------------------------------------------------------------------------------------------------------------------------------------------

kernel_time_t last_repeat = 0;      // time of the last command repetition
uint32_t repeat_interval_ms = 0;    // command repetition period
char repeat_command[64];            // repeated command

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// handles responding to commands from the DEBUG_UART
void debug_uart_task(void) {

    char tx_fifo[512];              // transmit fifo to be provided to the UART driver
    char rx_fifo[256];              // receive fifo to be provided to the UART driver
    char message_buffer[256];       // buffer for parsing an incomming message until a newline is received
    uint32_t buffer_pos = 0;        // current index into the message_buffer

    uart_init(DEBUG_UART, DEBUG_UART_BAUD, DEBUG_UART_TX_GPIO, DEBUG_UART_RX_GPIO, (char*)&tx_fifo, sizeof(tx_fifo), (char*)&rx_fifo, sizeof(rx_fifo));
    shell_print_header();

    while (1) {

        uint32_t bytes_parsed = 0;          // limit parsed bytes per update to avoid missed deadlines of other tasks or infinite loop in case of a fault

        // handle incomming communication via UART
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

        // handle command repetition
        if (repeat_interval_ms > 0 && kernel_get_time_since(last_repeat) >= repeat_interval_ms) {

            last_repeat = kernel_get_time_ms();
            shell_update(repeat_command);
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

// divides a number by 1000, converts it to string and sends it via DEBUG_UART with selected number of decimal places
void debug_print_int_dec(int value, uint8_t dec_places) {

    char string[16];        // stores the string generated from the input integer
    int buff_index = 0;     // write head to the string array
    int remainder = value;
    int i = 0;

    while (remainder > 0 || i < 3) {    // iterate while there are digits left or the decimal part isn't complete

        int digit = remainder % 10;     // get last digit
        remainder /= 10;

        // convert the current digit to a character and append it to the buffer; skip if the digit is ignored (dec_places setting)
        if (++i > 3 - dec_places) string[buff_index++] = digit + '0';

        // append a decimal point after the decimal part is finished
        if (i == 3) string[buff_index++] = '.';
    }        

    if (value < 1000) string[buff_index++] = '0';        // if the value doesn't have an integer part, append a zero

    // print the buffer (the characters are in reverse order)
    while (buff_index > 0) uart_putc(DEBUG_UART, string[--buff_index]);
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

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// start an automatic command repetition
void debug_repeat_start(char *command, uint32_t period) {

    last_repeat = 0;
    repeat_interval_ms = period;

    uint32_t len = strlen(command) + 1;
    if (len >= sizeof(repeat_command)) len = sizeof(repeat_command) - 1;

    memcpy(repeat_command, command, len);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// stop an automatic command repetition
void debug_repeat_stop(void) {

    repeat_interval_ms = 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
