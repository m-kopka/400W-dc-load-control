#include "common_defs.h"
#include "debug_uart.h"

#include "iset_dac.h"
#include "load_control.h"
#include "fan_control.h"
#include "temp_sensors.h"
#include "internal_isen.h"
#include "vi_sense.h"

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

    debug_print("----------------------------------------------------------\n");
    debug_print("  @@@@&        @@@@     @@@@   | 400W DC Electronic Load\n");
    debug_print("  @@@@@&      @@@@@   @@@@     | Control Board\n");
    debug_print("  @@@@@@@    @@@@@@ @@@@       | \n");
    debug_print("  @@@ @@@@  @@@ @@@@@@@@       | \n");
    debug_print("  @@@  @@@@@@@  @@@   @@@@     | \n");
    debug_print("  @@@   @@@@@   @@@    @@@@@   | Martin Kopka 2024\n");
    debug_print("  @@@    @@@    @@@      @@@@  | github.com/m-kopka\n");
    debug_print("----------------------------------------------------------\n");
    debug_print("(i) type \"help\" to show available commands.\n");
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// detects commands from the DEBUG_UART and respods to them
void shell_update(char *buffer) {

    if (buffer[0] == '\0') return;      // no command
    while ((buffer[0] <= ' ') && (buffer[0] != '\0')) buffer++;

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

    // sets the fan speed
    else if (SHELL_CMD("fan")) {

        shell_assert_argc(1);

        int speed = atoi(args[1]);
        if (speed >= 0 && speed < 256) {

            fan_set_speed_override(speed);

            debug_print("fan speed override set to ");
            debug_print(args[1]);
            debug_print(".\n");

        } else debug_print("fan speed range is <0 - 255>");
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

    // enables or disables the load
    else if (SHELL_CMD("en")) {

        shell_assert_argc(1);

        if (COMPARE_ARG(1, "0")) {

            load_set_enable(false);
            debug_print("load disabled.\n");
        }

        else if (COMPARE_ARG(1, "1")) {

            load_set_enable(true);
            debug_print("load enabled.\n");
        }

        else debug_print("invalid argument.\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // sets the load CC level
    else if (SHELL_CMD("iset")) {

        shell_assert_argc(1);

        int current = atoi(args[1]);
        if (current >= 0 && current <= 10000) {     // limit to 10A for now

            load_set_current((uint16_t)current);

            debug_print("CC level set to ");
            debug_print_int(current);
            debug_print(" mA.\n");
        }
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // sets the load CC level in raw 16bit DAC value (for calibration)
    else if (SHELL_CMD("iset-raw")) {

        shell_assert_argc(1);

        int code = atoi(args[1]);
        if (code >= 0x4000 && code <= 0xffff) {     // limit to 10A for now

            iset_dac_set_raw((uint16_t)code);

            debug_print("ISET_DAC code set to ");
            debug_print_int(code);
            debug_print(" lsb.\n");
        }
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // prints the load voltage
    else if (SHELL_CMD("vsen")) {

        debug_print("load input voltage: ");
        debug_print_int_dec(vi_sense_get_voltage(), 2);
        debug_print(" V\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // prints total load current
    else if (SHELL_CMD("isen")) {

        debug_print("load current: ");
        debug_print_int_dec(vi_sense_get_current(), 2);
        debug_print(" A\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // prints total load current
    else if (SHELL_CMD("psen")) {

        debug_print("load power: ");
        debug_print_int_dec(vi_sense_get_voltage() * vi_sense_get_current() / 1000, 2);
        debug_print(" W\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // prints all internal current sink currents
    else if (SHELL_CMD("isen-int")) {

        debug_print("i(L1): ");
        debug_print_int_dec(internal_isen_read(CURRENT_L1), 3);
        debug_print(" A\ni(L2): ");
        debug_print_int_dec(internal_isen_read(CURRENT_L2), 3);
        debug_print(" A\ni(R1): ");
        debug_print_int_dec(internal_isen_read(CURRENT_R1), 3);
        debug_print(" A\ni(R2): ");
        debug_print_int_dec(internal_isen_read(CURRENT_R2), 3);
        debug_print(" A\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    else if (SHELL_CMD("vsensrc")) {

        if (argc == 0) {

            debug_print("voltage sense source is ");
            debug_print((vi_sense_get_vsen_source() == VSEN_SRC_INTERNAL) ? "internal" : "remote");
            debug_print(".\n");

        } else {

            if (COMPARE_ARG(1, "internal")) {

                vi_sense_set_vsen_source(VSEN_SRC_INTERNAL);
                debug_print("voltage sense source set to internal\n");
        
            } else if (COMPARE_ARG(1, "remote")) {

                vi_sense_set_vsen_source(VSEN_SRC_REMOTE);
                debug_print("voltage sense source set to remote\n");

            }
        }
    
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // prints all internal current sink currents
    else if (SHELL_CMD("help")) {

        debug_print("list of available commands:\n");
        debug_print("reboot - restart the system\n");
        debug_print("fan <speed> - set the fan speed\n");
        debug_print("rpm - read the fan speed\n");
        debug_print("en <1 or 0> - enable or disable the load\n");
        debug_print("iset <current_mA> - set the load CC level\n");
        debug_print("vsen - read the load input voltage\n");
        debug_print("isen - read the total load current\n");
        debug_print("isen-int - read the individual current sink currents\n");
        debug_print("psen - read the load power\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    else debug_print("(!) unknown command.\n");
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
