#include "cmd_interface/load_cmd.h"
#include "load_control.h"
#include "vi_sense.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

void load_cmd_task(void) {

    kernel_time_t last_watchdog_reload = 0;

    cmd_driver_init();
    cmd_write(CMD_ADDRESS_ID, LOAD_ID_CODE);

    while (1) {

        if (cmd_has_data()) {

            uint8_t address;
            uint16_t data;
            bool checksum_correct = cmd_read(&address, &data);

            if (checksum_correct) {

                if (address == CMD_ADDRESS_CONFIG) {

                    if (data & LOAD_CONFIG_AUTO_VSEN_SRC) vi_sense_set_automatic_vsen_source(true);
                    else if (data & LOAD_CONFIG_VSEN_SRC) {
                        
                        vi_sense_set_automatic_vsen_source(false);
                        vi_sense_set_vsen_source(VSEN_SRC_REMOTE);
                    
                    } else {
                        
                        vi_sense_set_automatic_vsen_source(false);
                        vi_sense_set_vsen_source(VSEN_SRC_INTERNAL);
                    }
                }

                if (address == CMD_ADDRESS_ENABLE) {

                    if (data == LOAD_ENABLE_KEY) load_set_enable(true);
                    else load_set_enable(false);
                }

                if (address == CMD_ADDRESS_CC_LEVEL) {

                    if (data <= LOAD_MAX_CURRENT_MA) load_set_cc_level(data);
                }

                if (address == CMD_ADDRESS_WD_RELOAD) {

                    if (data == LOAD_WD_RELOAD_KEY) last_watchdog_reload = kernel_get_time_ms();
                }

            }
        }

        if (load_get_status() & LOAD_STATUS_ENABLED && kernel_get_time_since(last_watchdog_reload) >= LOAD_WD_TIMEOUT_MS) {

            load_trigger_fault(LOAD_FAULT_COM);
        }

        kernel_yield();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
