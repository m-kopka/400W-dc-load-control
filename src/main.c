
/*
 *  400W electronic load control board firmware
 *  Martin Kopka 2024
*/

#include "common_defs.h"
#include "hal/iwdg.h"
#include "load_control.h"
#include "temp_control.h"
#include "cmd_interface/load_cmd.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

void watchdog_task(void);

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

int main() {

    rcc_enable_peripheral_clock(RCC_PERIPH_APB1_PWR);
    write_masked(PWR->CR, 0x3 < 14, 0x3 << 14);                                     // voltage regulator scale 1
    set_bits(FLASH->ACR, FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN);       // flash prefetch, instruction cache, data cache enable
    write_masked(FLASH->ACR, 3 << 0, 0xf << 0);                                     // flash latency 3 wait states (4 CPU cycles)
    
    // enable the High Speed External oscillator, setup the PLL to use the HSE as an input clock and output a 96MHz system clock
    rcc_enable_hse(HSE_OSC_FREQUENCY_HZ);
    rcc_set_bus_prescalers(RCC_SYS_CLOCK_DIV1, RCC_PERIPH_CLOCK_DIV2, RCC_PERIPH_CLOCK_DIV1);
    rcc_pll_init(CORE_CLOCK_FREQUENCY_HZ, RCC_PLL_SOURCE_HSE);
    rcc_set_system_clock_source(RCC_SYSTEM_CLOCK_SOURCE_PLL);

    uint32_t watchdog_stack[32];
    uint32_t debug_uart_stack[512];
    uint32_t temp_control_stack[512];
    uint32_t load_control_stack[256];
    uint32_t load_cmd_stack[256];

    kernel_init(HLCK_frequency_hz);
    kernel_create_task(watchdog_task, watchdog_stack, sizeof(watchdog_stack), 500);
    kernel_create_task(debug_uart_task, debug_uart_stack, sizeof(debug_uart_stack), 2000);
    kernel_create_task(temp_control_task, temp_control_stack, sizeof(temp_control_stack), 100);
    kernel_create_task(load_control_task, load_control_stack, sizeof(load_control_stack), 100);
    kernel_create_task(load_cmd_task, load_cmd_stack, sizeof(load_cmd_stack), 100);
    kernel_start();

    while (1) NVIC_SystemReset();     // kernel crashed
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// handles the periodic watchdog reloading
void watchdog_task(void) {

    iwdg_init(IWDG_PR_8, 0xfff);        // timeout is about 1024ms. Task deadline is 500ms to guarantee a reload before the watchdog runs out

    while (1) {

        iwdg_reload();
        kernel_yield();
    }
}

//---- IRQ HANDLERS ----------------------------------------------------------------------------------------------------------------------------------------------

void HardFault_Handler() {

    NVIC_SystemReset();
    while(1);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
