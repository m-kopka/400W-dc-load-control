
/*
 *  400W electronic load control board firmware
 *  Martin Kopka 2024
*/

#include "common_defs.h"
#include "load_control.h"
#include "temp_control.h"

void heartbeat_task(void);

int main() {

    rcc_enable_peripheral_clock(RCC_PERIPH_APB1_PWR);
    write_masked(PWR->CR, 0x3 < 14, 0x3 << 14);                                     // voltage regulator scale 1
    set_bits(FLASH->ACR, FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN);       // flash prefetch, instruction cache, data cache enable
    write_masked(FLASH->ACR, 3 << 0, 0xf << 0);                                     // flash latency 3 wait states (4 CPU cycles)
    rcc_enable_hse(HSE_OSC_FREQUENCY_HZ);
    rcc_set_bus_prescalers(RCC_SYS_CLOCK_DIV1, RCC_PERIPH_CLOCK_DIV2, RCC_PERIPH_CLOCK_DIV1);
    rcc_pll_init(CORE_CLOCK_FREQUENCY_HZ, RCC_PLL_SOURCE_HSE);
    rcc_set_system_clock_source(RCC_SYSTEM_CLOCK_SOURCE_PLL);

    rcc_enable_peripheral_clock(RCC_PERIPH_AHB1_GPIOC);
    gpio_set_mode(GPIOC, 14, GPIO_MODE_OUTPUT);

    kernel_init(HLCK_frequency_hz);
    kernel_create_task(debug_uart_task, 2000);
    kernel_create_task(heartbeat_task, 5000);
    kernel_create_task(temp_control_task, 100);
    kernel_create_task(load_control_task, 100);
    kernel_start();

    while (1) {

        NVIC_SystemReset();     // kernel crashed
    }
}

void heartbeat_task(void) {

    rcc_enable_peripheral_clock(LED_RED_GPIO_CLOCK);
    gpio_write(LED_RED_GPIO, HIGH);
    gpio_set_mode(LED_RED_GPIO, GPIO_MODE_OUTPUT);

    while (1) {

        gpio_write(LED_RED_GPIO, LOW);
        kernel_sleep_ms(50);
        gpio_write(LED_RED_GPIO, HIGH);
        kernel_sleep_ms(1950);
    }
}

void HardFault_Handler() {

    NVIC_SystemReset();
    while(1);
}
