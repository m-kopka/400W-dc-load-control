#include "common_defs.h"
#include "debug_uart.h"

#include "fan_control.h"
#include "temp_sensors.h"

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

#define SHELL_CMD(command) (strcmp(args[0], (command)) == 0)

#define COMPARE_ARG(argnum, str) (strcmp(args[(argnum)], (str)) == 0)

// prints an error message if the user did't provide enough arguments for the command
#define shell_assert_argc(count) if (argc < count + 1) { \
                                    debug_print("too few arguments for \""); \
                                    debug_print(args[0]); \
                                    debug_print("\"\n"); \
                                    return; \
                                }

uint8_t shell_parse_args(char *input, char **args, uint8_t argc);

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// prints the shell header on startup
void shell_print_header(void) {

    debug_print("---------------------------------------------------\n");
    debug_print("| DC Electronic Load\n");
    debug_print("| Martin Kopka 2024\n");
    debug_print("---------------------------------------------------\n");
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// detects commands from the DEBUG_UART and respods to them
void shell_update(char *buffer) {

    while ((buffer[0] <= ' ') && (buffer[0] != '\0')) buffer++;

    debug_print("> ");
    debug_print(buffer);
    debug_print("\n");

    char *args[5];
    uint8_t argc = shell_parse_args(buffer, args, 5);

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    if (SHELL_CMD("ahoj")) {

        debug_print("zdar jak svina\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // resets the processor
    else if (SHELL_CMD("reboot")) {

        NVIC_SystemReset();
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // sets the fan speeds
    else if (SHELL_CMD("fan")) {

        shell_assert_argc(1);

        int speed = atoi(args[1]);
        if (speed >= 0 && speed < 256) {

            fan_set_speed_override(speed);

            debug_print("fan speed set to ");
            debug_print(args[1]);
            debug_print(".\n");
        } 
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // returns the fan RPM
    else if (SHELL_CMD("rpm")) {

        for (int fan = 0; fan < 2; fan++) {

            debug_print((fan == FAN1) ? "fan speeds FAN1: " : ", FAN2: ");
            debug_print_int(fan_get_rpm(fan));
            debug_print("rpm");
        }

        debug_print(".\n"); 
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // returns the power transistor temperatures
    else if (SHELL_CMD("temp")) {

        temp_sensor_init();     // nahradit

        for (int sensor = 0; sensor < 2; sensor++) {

            debug_print((sensor == TEMP_L) ? "temperatures L: " : ", R: ");

            uint8_t temperature = temp_sensor_read(sensor);
            temp_sensor_fault_t fault = temp_sensor_read_faults(sensor);

            if (fault == TEMP_SEN_FAULT_OPEN) debug_print("OPEN");
            if (fault == TEMP_SEN_FAULT_SHORT) debug_print("SHORT");
            if (fault == TEMP_SEN_FAULT_NONE) {
            
                debug_print_int(temperature);
                debug_print("'C");
            }
        }

        debug_print(".\n"); 
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// separates an input string into a command and arguments all separated by spaces; arguments with spaces are possible by using quotation marks
// caution: the original string is modified by the function; the function inserts null-terminators after each argument and returns pointers to the arguments in the original string
uint8_t shell_parse_args(char *input, char **args, uint8_t argc) {

    uint8_t  current_arg = 0;                       // index of current argument
    bool     parsing = false;                       // currently parsing an argument, not looking for a beginning of a new argument
    bool     prev_was_space = true;                 // previous char was a space
    bool     parse_until_quotation_mark = false;    // current argument started with a quotation mark, parse until next quotation mark and ignore spaces

    while (*input != '\0') {

        // detect the end of current argument
        if ((!parse_until_quotation_mark && !prev_was_space && (*input == ' ')) ||      // argument didn't start with a quotation mark, previous char was alphanumerical and current char is a space (argument <--)
             (parse_until_quotation_mark && *input == '\"')) {                          // argument started with a quotation mark and current char is the closing quotation mark ("argument"<--)

            *input = '\0';             // NULL terminate the argument in the input buffer
            parse_until_quotation_mark = false;
            parsing = false;

            if (current_arg == argc) break;     // stop parsing after the end of last argument was detected
        }

        // detect the start of a new argument
        // we are currently not parsing an argument in quotation marks, previous char was a space, current char is alphanumerical
        if (prev_was_space && (*input > ' ')) {

            if (*input == '\"') parse_until_quotation_mark = true;                  // argument started with a quotation mark
            else if (!parsing) {
                
                args[current_arg++] = input;      // mark start of a new argument
                parsing = true;
            }
        }

        prev_was_space = (*input == '\0') || (*input == ' ') || (*input == '\"');
        input++;
    }

    *input = '\0';

    return (current_arg);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
