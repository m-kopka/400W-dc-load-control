#include "common_defs.h"
#include "debug_uart.h"

#include "iset_dac.h"
#include "load_control.h"
#include "fan_control.h"
#include "temp_sensors.h"
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

uint8_t __parse_args(char *input, char **args, uint8_t argc);
uint32_t __parse_hex_string(char *string);

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
    uint8_t argc = __parse_args(buffer, args, 5);

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    if (SHELL_CMD("ahoj")) debug_print("zdar jak sviňa\n");

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // periodically repeats a command
    else if (SHELL_CMD("repeat")) {

        if (argc == 1) debug_repeat_stop();

        else if (argc >= 3) {

            int period = atoi(args[1]);

            if (period >= 10) debug_repeat_start(args[2], period);
            else debug_print("(!) minimum period is 10 ms.\n");

        } else {

            debug_print("(!) usage: repeat \"<command>\" <period_ms>\n");
        }
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // resets the processor
    else if (SHELL_CMD("reboot")) {

        debug_print("restarting..\n");
        kernel_sleep_ms(200);
        NVIC_SystemReset();
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // prints the load status
    else if (SHELL_CMD("status")) {

        uint16_t status = load_get_status();
        uint16_t faults = load_get_faults(LOAD_FAULT_ALL);
        uint16_t fault_mask = load_get_fault_mask();

        debug_print("\nLOAD STATUS\n");
        debug_print("load is ");
        debug_print((status & LOAD_STATUS_ENABLED) ? "ENABLED\n" : "DISABLED\n");
        debug_print("fault state is ");
        debug_print((status & LOAD_STATUS_FAULT) ? "ACTIVE\n" : "not active\n");

        debug_print("\nCOM fault\t");
        debug_print((faults & LOAD_FAULT_COM) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_COM) ? "(enabled)\n" : "(masked)\n");
        debug_print("OTP fault\t");
        debug_print((faults & LOAD_FAULT_OTP) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_OTP) ? "(enabled)\n" : "(masked)\n");
        debug_print("OCP fault\t");
        debug_print((faults & LOAD_FAULT_OCP) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_OCP) ? "(enabled)\n" : "(masked)\n");
        debug_print("OPP fault\t");
        debug_print((faults & LOAD_FAULT_OPP) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_OPP) ? "(enabled)\n" : "(masked)\n");
        debug_print("TEMP_L fault\t");
        debug_print((faults & LOAD_FAULT_TEMP_L) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_TEMP_L) ? "(enabled)\n" : "(masked)\n");
        debug_print("TEMP_R fault\t");
        debug_print((faults & LOAD_FAULT_TEMP_R) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_TEMP_R) ? "(enabled)\n" : "(masked)\n");
        debug_print("FAN1 fault\t");
        debug_print((faults & LOAD_FAULT_FAN1) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_FAN1) ? "(enabled)\n" : "(masked)\n");
        debug_print("FAN2 fault\t");
        debug_print((faults & LOAD_FAULT_FAN2) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_FAN2) ? "(enabled)\n" : "(masked)\n");
        debug_print("FUSE_L1 fault\t");
        debug_print((faults & LOAD_FAULT_FUSE_L1) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_FUSE_L1) ? "(enabled)\n" : "(masked)\n");
        debug_print("FUSE_L2 fault\t");
        debug_print((faults & LOAD_FAULT_FUSE_L2) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_FUSE_L2) ? "(enabled)\n" : "(masked)\n");
        debug_print("FUSE_R1 fault\t");
        debug_print((faults & LOAD_FAULT_FUSE_R1) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_FUSE_R1) ? "(enabled)\n" : "(masked)\n");
        debug_print("FUSE_R2 fault\t");
        debug_print((faults & LOAD_FAULT_FUSE_R2) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_FUSE_R2) ? "(enabled)\n" : "(masked)\n");
        debug_print("EXTERNAL fault\t");
        debug_print((faults & LOAD_FAULT_EXTERNAL) ? "ACTIVE\t\t" : "not active\t");
        debug_print((fault_mask & LOAD_FAULT_EXTERNAL) ? "(enabled)\n" : "(masked)\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // clears all load fault flags
    else if (SHELL_CMD("clearfaults")) {

        load_clear_fault(LOAD_FAULT_ALL);
        debug_print("all faults cleared.\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    else if (SHELL_CMD("faultmask")) {

        shell_assert_argc(1);
        int mask = __parse_hex_string(args[1]);

        if (mask >= 0 && mask <= 0xffff) {

            load_set_fault_mask((uint16_t)mask);

            debug_print("fault mask changed to ");
            debug_print_int_hex(load_get_fault_mask(), 4);
            debug_print(".\n");
        }
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // enables or disables the load; returns the load enable state if the command is used without an argument
    else if (SHELL_CMD("en")) {

        uint16_t status_reg = load_get_status();

        if (argc == 1) {

            debug_print("load is ");
            debug_print((status_reg & LOAD_STATUS_ENABLED) ? "enabled.\n" : "disabled.\n");

        } else {

            if (!(status_reg & LOAD_STATUS_READY)) debug_print("(!) load is not ready.\n");
            else if (status_reg & LOAD_STATUS_FAULT) debug_print("(!) load is in the fault state.\n");

            else if (COMPARE_ARG(1, "0")) {

                if (!(status_reg & LOAD_STATUS_ENABLED)) debug_print("load is already disabled.\n");
                else {

                    load_set_enable(false);
                    debug_print("load disabled.\n");
                }
            }

            else if (COMPARE_ARG(1, "1")) {

                if (status_reg & LOAD_STATUS_ENABLED) debug_print("load is already enabled.\n");
                else {

                   load_set_enable(true);
                    debug_print("load enabled.\n"); 
                }
            }

            else debug_print("(!) invalid argument. Use \"1\" to enable or \"0\" to disable.\n");
        }
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // sets the load CC level
    else if (SHELL_CMD("iset")) {

        shell_assert_argc(1);
        int current = atoi(args[1]);

        if (current >= LOAD_MIN_CURRENT_MA && current <= LOAD_MAX_CURRENT_MA) {

            load_set_cc_level((uint16_t)current);

            debug_print("CC level set to ");
            debug_print_int(current);
            debug_print(" mA.\n");

        } else {

            debug_print("(!) iset range is <");
            debug_print_int(LOAD_MIN_CURRENT_MA);
            debug_print(" - ");
            debug_print_int(LOAD_MAX_CURRENT_MA);
            debug_print(">.\n");
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

    // prints all internal current sink currents
    else if (SHELL_CMD("isen-int")) {

        debug_print("i(L1): ");
        debug_print_int_dec(internal_isen_read(CURRENT_L1), 2);
        debug_print(" A\ni(L2): ");
        debug_print_int_dec(internal_isen_read(CURRENT_L2), 2);
        debug_print(" A\ni(R1): ");
        debug_print_int_dec(internal_isen_read(CURRENT_R1), 2);
        debug_print(" A\ni(R2): ");
        debug_print_int_dec(internal_isen_read(CURRENT_R2), 2);
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

    // selects the voltage sense source
    else if (SHELL_CMD("vsensrc")) {

        if (argc == 1) {

            debug_print("voltage sense source is ");
            debug_print((vi_sense_get_vsen_source() == VSEN_SRC_INTERNAL) ? "internal" : "remote");
            debug_print(".\n");

        } else {

            if (COMPARE_ARG(1, "internal")) {

                vi_sense_set_automatic_vsen_source(false);
                vi_sense_set_vsen_source(VSEN_SRC_INTERNAL);
                debug_print("voltage sense source set to internal\n");
        
            } else if (COMPARE_ARG(1, "remote")) {

                vi_sense_set_automatic_vsen_source(false);
                vi_sense_set_vsen_source(VSEN_SRC_REMOTE);
                debug_print("voltage sense source set to remote\n");

            } else if (COMPARE_ARG(1, "auto")) {

                vi_sense_set_automatic_vsen_source(true);
                debug_print("voltage sense source set to auto\n");

            } else debug_print("(!) invalid argument. Use \"internal\", \"remote\" or \"auto\".\n");
        }
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // sets the automatic disable voltage level
    else if (SHELL_CMD("vdis")) {

        shell_assert_argc(1);
        int voltage = atoi(args[1]);

        if (voltage >= 0 && voltage <= 100000) {

            load_set_discharge_voltage((uint32_t)voltage);

            if (voltage > 0) {

                debug_print("discharge voltage set to: ");
                debug_print_int(voltage);
                debug_print(" mV.\n");

            } else debug_print("loat auto-disable feature disabled.\n");    
        }
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // returns the power transistor temperatures
    else if (SHELL_CMD("temp")) {

        for (int sensor = TEMP_L; sensor <= TEMP_R; sensor++) {

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

    // sets the fan speed
    else if (SHELL_CMD("fan")) {

        shell_assert_argc(1);

        int speed = atoi(args[1]);
        if (speed >= 0 && speed < 256) {

            fan_set_pwm_override(speed);

            debug_print("fan pwm override set to ");
            debug_print(args[1]);
            debug_print(".\n");

        } else debug_print("fan pwm range is <0 - 255>");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // returns the fan RPM
    else if (SHELL_CMD("rpm")) {

        for (int fan = 0; fan < 2; fan++) {

            debug_print((fan == FAN1) ? "fan speeds FAN1: " : ", FAN2: ");
            debug_print_int(fan_get_rpm(fan));
            debug_print("rpm");
        }

        debug_print(", pwm: "); 
        debug_print_int((uint16_t)fan_get_pwm() * 100 / 255);
        debug_print(" %.\n"); 
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    // prints available commands
    else if (SHELL_CMD("help")) {

        debug_print("\nlist of available commands:\n");
        debug_print("repeat <period_ms> <command> - periodically repeat a command\n");
        debug_print("reboot - restart the system\n");
        debug_print("status - print the load status\n");
        debug_print("clearfaults - clear all load fault flags\n");
        debug_print("faultmask <0000 - ffff> - set a fault mask\n");
        debug_print("en <1 or 0> - enables or disables the load\n");
        debug_print("iset <current_ma> - set the load CC level\n");
        debug_print("vsen - read the load input voltage\n");
        kernel_sleep_ms(50);
        debug_print("isen - read the total load current\n");
        debug_print("isen-int - read the individual current sink currents\n");
        debug_print("psen - read the load power\n");
        debug_print("vsensrc <internal or remote> - set the voltage sense source\n");
        debug_print("vdis <voltage_mv> - disable the load automatically when the source voltage drops bellow a threshold\n");
        debug_print("temp - read the power transistor temperatures\n");
        debug_print("fan <0 - 255> - set the fan pwm\n");
        debug_print("rpm - read the fan speed\n");
    }

    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

    else debug_print("(!) unknown command.\n");
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// separates an input string into a command and arguments all separated by spaces; arguments with spaces are possible by using quotation marks
// caution: the original string is modified by the function; the function inserts null-terminators after each argument and returns pointers to the arguments in the original string
uint8_t __parse_args(char *input, char **args, uint8_t argc) {

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

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// converts a string containing a hexadecimal number to an unsigned integer
uint32_t __parse_hex_string(char *string) {

    uint32_t result = 0;

    for (int i = 0; string[i] != '\0'; ++i) {
        
        if      (string[i] >= '0' && string[i] <= '9') result = result * 16 + string[i] - '0';
        else if (string[i] >= 'a' && string[i] <= 'f') result = result * 16 + 10 + string[i] - 'a';
        else if (string[i] >= 'A' && string[i] <= 'F') result = result * 16 + 10 + string[i] - 'A';
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
