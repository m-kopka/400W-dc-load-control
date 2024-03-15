#include "cmd_interface/load_cmd.h"
#include "load_control.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

void load_cmd_task(void) {

    cmd_driver_init();
    cmd_write(CMD_ADDRESS_ID, 0x10AD);

    while (1) {

        if (cmd_has_data()) {

            uint8_t address;
            uint16_t data;
            bool checksum_correct = cmd_read(&address, &data);

            if (checksum_correct) {

                if (address == CMD_ADDRESS_ENABLE) {

                    if (data == 0xABCD) load_set_enable(true);
                    else load_set_enable(false);
                }

                if (address == CMD_ADDRESS_CC_LEVEL) {

                    if (data <= 10000) load_set_cc_level(data);
                }

            }
        }

        kernel_yield();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
