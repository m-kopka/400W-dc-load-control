#include "cmd_spi_task.h"
#include "cmd_spi_driver.h"
#include "load_control.h"
#include "vi_sense.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// handles write commands from master and communication watchdog timeout
void load_cmd_task(void) {

    kernel_time_t last_watchdog_reload = 0;     // absolute time of last watchdog reload write command

    cmd_driver_init();
    cmd_write(CMD_ADDRESS_ID, LOAD_ID_CODE);

    while (1) {

        // handle write commands from the master if the RX fifo is not empty
        if (cmd_has_data()) {

            uint8_t address;
            uint16_t data;
            bool checksum_correct = cmd_read(&address, &data);

            if (checksum_correct) {

                switch (address) {

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // Load Configuration Register
                    case CMD_ADDRESS_CONFIG: {

                        load_set_mode(data & 0x3);      // lower 2 bits are Load Mode

                        // if the auto vsen src is not selected, disable it and select a source according to the CONFIG_VSEN_SRC bit
                        if (data & LOAD_CONFIG_AUTO_VSEN_SRC) vi_sense_set_automatic_vsen_source(true);
                        else if (data & LOAD_CONFIG_VSEN_SRC) {
                        
                            vi_sense_set_automatic_vsen_source(false);
                            vi_sense_set_vsen_source(VSEN_SRC_REMOTE);
                    
                        } else {
                        
                            vi_sense_set_automatic_vsen_source(false);
                            vi_sense_set_vsen_source(VSEN_SRC_INTERNAL);
                        }

                    } break;

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // Load Fault Mask register
                    case CMD_ADDRESS_FAULT_MASK: {

                        load_set_fault_mask(data);

                    } break;

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // Load Watchdog Reload register
                    case CMD_ADDRESS_WD_RELOAD: {

                        if (data == LOAD_WD_RELOAD_KEY) last_watchdog_reload = kernel_get_time_ms();

                    } break;

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // Load Enable register
                    case CMD_ADDRESS_ENABLE: {

                        if (data == LOAD_ENABLE_KEY) load_set_enable(true);
                        else load_set_enable(false);

                    } break;

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // Load CC Level register
                    case CMD_ADDRESS_CC_LEVEL: {

                        load_set_cc_level(data);

                    } break;

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // Load CV Level register
                    case CMD_ADDRESS_CV_LEVEL: {

                        load_set_cv_level(data * 10);

                    } break;

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // Load CR Level register
                    case CMD_ADDRESS_CR_LEVEL: {

                        load_set_cr_level(data);

                    } break;

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // Load CP Level register
                    case CMD_ADDRESS_CP_LEVEL: {

                        load_set_cp_level(data * 100);

                    } break;

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

                    // Load Discharge Voltage register
                    case CMD_ADDRESS_DISCH_LEVEL: {

                        load_set_discharge_voltage(data);

                    } break;

                    //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
                }
            }
        }

        // trigger a communication fault if the watchdog timer runs out
        if (load_get_status() & LOAD_STATUS_ENABLED && kernel_get_time_since(last_watchdog_reload) >= LOAD_WD_TIMEOUT_MS) {

            load_trigger_fault(LOAD_FAULT_COM);
        }

        kernel_yield();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
