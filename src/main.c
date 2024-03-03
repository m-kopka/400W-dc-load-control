#include "common_defs.h"

int main() {

    rcc_enable_peripheral_clock(RCC_PERIPH_APB1_PWR);
    write_masked(PWR->CR, 0x3 < 14, 0x3 << 14);                                     // voltage regulator scale 1
    set_bits(FLASH->ACR, FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN);       // flash prefetch, instruction cache, data cache enable
    write_masked(FLASH->ACR, 3 << 0, 0xf << 0);                                     // flash latency 3 wait states (4 CPU cycles)
    rcc_enable_hse(25000000);
    rcc_set_bus_prescalers(RCC_SYS_CLOCK_DIV1, RCC_PERIPH_CLOCK_DIV2, RCC_PERIPH_CLOCK_DIV1);
    rcc_pll_init(96000000, RCC_PLL_SOURCE_HSE);
    rcc_set_system_clock_source(RCC_SYSTEM_CLOCK_SOURCE_PLL);

    rcc_enable_peripheral_clock(RCC_PERIPH_AHB1_GPIOC);
    gpio_set_mode(GPIOC, 14, GPIO_MODE_OUTPUT);

    while (1) {

        gpio_toggle(GPIOC, 14);
        for (volatile int i = 0; i < 10000000; i++);
    }
}

void HardFault_Handler() {

    NVIC_SystemReset();
    while(1);
}
