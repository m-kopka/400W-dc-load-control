#include "load_control.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

void ext_fault_task(void) {

    rcc_enable_peripheral_clock(EXT_FAULT_GPIO_CLOCK);
    gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_INPUT);

    while (1) {

        static uint8_t debounce_counter = 0xff;
        static bool triggered = false;

        debounce_counter = (debounce_counter << 1) | gpio_get(EXT_FAULT_GPIO);

        if (debounce_counter == 0x00) {

            if (!triggered) {

                load_trigger_fault(LOAD_FAULT_EXTERNAL);
                triggered = true;
            }

        } else if (debounce_counter == 0xff) triggered = false;

        kernel_yield();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
