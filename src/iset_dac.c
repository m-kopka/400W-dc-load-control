#include "iset_dac.h"
#include "hal/spi.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the SPI communication with the I_SET DAC and sets it to the 0A current level
void iset_dac_init(void) {

    rcc_enable_peripheral_clock(ISET_DAC_SPI_CLOCK);
    rcc_enable_peripheral_clock(ISET_DAC_SPI_SS_GPIO_CLOCK);
    rcc_enable_peripheral_clock(ISET_DAC_SPI_SCK_GPIO_CLOCK);
    rcc_enable_peripheral_clock(ISET_DAC_SPI_MOSI_GPIO_CLOCK);

    gpio_write(ISET_DAC_SPI_SS_GPIO, HIGH);
    gpio_set_mode(ISET_DAC_SPI_SS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_mode(ISET_DAC_SPI_SCK_GPIO, GPIO_MODE_ALTERNATE_FUNCTION);
    gpio_set_mode(ISET_DAC_SPI_MOSI_GPIO, GPIO_MODE_ALTERNATE_FUNCTION);

    gpio_set_alternate_function(ISET_DAC_SPI_SCK_GPIO, ISET_DAC_GPIO_AF);
    gpio_set_alternate_function(ISET_DAC_SPI_MOSI_GPIO, ISET_DAC_GPIO_AF);

    // software slave management, 16bit format, master mode, baud divisor
    ISET_DAC_SPI->CR1  = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_DFF | SPI_CR1_MSTR | ISET_DAC_SPI_DIV;
    ISET_DAC_SPI->CR2  = 0;
    ISET_DAC_SPI->CR1 |= SPI_CR1_SPE;       // spi enable

    iset_dac_set_current(0);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the I_SET DAC output voltage to the corresponding current value; returns the real set current in mA clamped and rounded by the driver
uint32_t iset_dac_set_current(uint32_t current_ma) {

    uint32_t lut_entry = current_ma / 100;
    if (lut_entry > 400) lut_entry = 400;

    int32_t code = -14836 * (int32_t)current_ma / 10000 + 62569;
    iset_dac_set_raw(code);

    return (lut_entry * 100);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the specified code to the ISET_DAC
void iset_dac_set_raw(uint16_t code) {

    kernel_time_t start_time = kernel_get_time_ms();

    gpio_write(ISET_DAC_SPI_SS_GPIO, LOW);
    spi_write(ISET_DAC_SPI, code);

    while (!spi_tx_done(ISET_DAC_SPI) && kernel_get_time_since(start_time) < 10);
    gpio_write(ISET_DAC_SPI_SS_GPIO, HIGH);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
