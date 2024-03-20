#include "vi_sense.h"
#include "hal/spi.h"
#include "cmd_interface/cmd_spi_driver.h"

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

uint16_t __vsen_adc_read(void);
uint16_t __isen_adc_read(void);

//---- INTERNAL DATA ---------------------------------------------------------------------------------------------------------------------------------------------

uint32_t load_voltage_mv = 0;                   // current load voltage [mV]. Updated by the vi_sense_task
uint32_t load_current_ma = 0;                   // current load current [mA]. Updated by the vi_sense_task
vsen_src_t vsen_src = VSEN_SRC_INTERNAL;        // voltage sense source (internal or remote)

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// samples load voltage and current and rounds the results
void vi_sense_task(void) {

    //---- VOLTAGE SENSE ADC SPI INIT ----------------------------------------------------------------------------------------------------------------------------
    
    rcc_enable_peripheral_clock(VSEN_ADC_SPI_CLOCK);
    rcc_enable_peripheral_clock(VSEN_ADC_SPI_SS_GPIO_CLOCK);
    rcc_enable_peripheral_clock(VSEN_ADC_SPI_SCK_GPIO_CLOCK);
    rcc_enable_peripheral_clock(VSEN_ADC_SPI_MISO_GPIO_CLOCK);

    gpio_write(VSEN_ADC_SPI_SS_GPIO, HIGH);
    gpio_set_mode(VSEN_ADC_SPI_SS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_mode(VSEN_ADC_SPI_SCK_GPIO, GPIO_MODE_ALTERNATE_FUNCTION);
    gpio_set_mode(VSEN_ADC_SPI_MISO_GPIO, GPIO_MODE_ALTERNATE_FUNCTION);
    gpio_set_alternate_function(VSEN_ADC_SPI_SCK_GPIO, VSEN_ADC_GPIO_AF);
    gpio_set_alternate_function(VSEN_ADC_SPI_MISO_GPIO, VSEN_ADC_GPIO_AF);

    // software slave management, 16bit format, master mode, baud divisor
    VSEN_ADC_SPI->CR1  = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_DFF | SPI_CR1_MSTR | VSEN_ADC_SPI_DIV;
    VSEN_ADC_SPI->CR2  = 0;
    VSEN_ADC_SPI->CR1 |= SPI_CR1_SPE;       // spi enable

    //---- VOLTAGE SENSE ADC CONVST INIT -------------------------------------------------------------------------------------------------------------------------

    rcc_enable_peripheral_clock(VSEN_ADC_CONVST_GPIO_CLOCK);
    gpio_set_mode(VSEN_ADC_CONVST_GPIO, GPIO_MODE_OUTPUT);
    gpio_write(VSEN_ADC_CONVST_GPIO, HIGH);

    //---- VOLTAGE SENSE SOURCE INIT -----------------------------------------------------------------------------------------------------------------------------

    rcc_enable_peripheral_clock(VSEN_SRC_GPIO_CLOCK);
    gpio_set_mode(VSEN_SRC_GPIO, GPIO_MODE_OUTPUT);
    gpio_write(VSEN_SRC_GPIO, LOW);

    //---- CURRENT SENSE ADC SPI INIT ----------------------------------------------------------------------------------------------------------------------------

    rcc_enable_peripheral_clock(ISEN_ADC_SPI_CLOCK);
    rcc_enable_peripheral_clock(ISEN_ADC_SPI_SS_GPIO_CLOCK);
    rcc_enable_peripheral_clock(ISEN_ADC_SPI_SCK_GPIO_CLOCK);
    rcc_enable_peripheral_clock(ISEN_ADC_SPI_MISO_GPIO_CLOCK);

    gpio_write(ISEN_ADC_SPI_SS_GPIO, HIGH);
    gpio_set_mode(ISEN_ADC_SPI_SS_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_mode(ISEN_ADC_SPI_SCK_GPIO , GPIO_MODE_ALTERNATE_FUNCTION);
    gpio_set_mode(ISEN_ADC_SPI_MISO_GPIO, GPIO_MODE_ALTERNATE_FUNCTION);

    gpio_set_alternate_function(ISEN_ADC_SPI_SCK_GPIO, ISEN_ADC_GPIO_AF);
    gpio_set_alternate_function(ISEN_ADC_SPI_MISO_GPIO, ISEN_ADC_GPIO_AF);

    // software slave management, 16bit format, master mode, baud divisor
    ISEN_ADC_SPI->CR1  = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_DFF | SPI_CR1_MSTR | ISEN_ADC_SPI_DIV;
    ISEN_ADC_SPI->CR2  = 0;
    ISEN_ADC_SPI->CR1 |= SPI_CR1_SPE;       // spi enable

    //---- CURRENT SENSE ADC CONVST INIT -------------------------------------------------------------------------------------------------------------------------

    rcc_enable_peripheral_clock(ISEN_ADC_CONVST_GPIO_CLOCK);
    gpio_set_mode(ISEN_ADC_CONVST_GPIO, GPIO_MODE_OUTPUT);
    gpio_write(ISEN_ADC_CONVST_GPIO, HIGH);

    //------------------------------------------------------------------------------------------------------------------------------------------------------------

    internal_isen_init();

    // wait for the voltages to settle after power-up and perform first dummy read to trigger the next conversion
    kernel_sleep_ms(500);
    __vsen_adc_read();
    __isen_adc_read();

    while (1) {

        static uint32_t vsen_sample_sum = 0;    // sum of unprocessed voltage samples
        static uint32_t isen_sample_sum = 0;    // sum od unprocessed current samples
        static uint8_t sample_count = 0;        // number of samples contained in the vsen and isen sample_sum

        // get new samples
        vsen_sample_sum += __vsen_adc_read();
        isen_sample_sum += __isen_adc_read();
        sample_count++;

        if (sample_count == 16) {

            // divide sample sum by sample count and convert to mV and mA
            uint16_t vsen_sample = (vsen_src == VSEN_SRC_INTERNAL) ? VSEN_ADC_CODE_TO_MV_INT(vsen_sample_sum >> 4) : VSEN_ADC_CODE_TO_MV_REM(vsen_sample_sum >> 4);
            uint16_t isen_sample = ISEN_ADC_CODE_TO_MA(isen_sample_sum >> 4);

            // calculate moving average
            load_voltage_mv = (load_voltage_mv + vsen_sample) >> 1;
            load_current_ma = (load_current_ma + isen_sample) >> 1;

            // round to 50mV / 50mA
            load_voltage_mv = (load_voltage_mv + 25) / 50 * 50;
            load_current_ma = (load_current_ma + 25) / 50 * 50;

            if (load_voltage_mv < VI_SENSE_MIN_VOLTAGE) load_voltage_mv = 0;
            if (load_current_ma < VI_SENSE_MIN_CURRENT) load_current_ma = 0;

            // update registers
            cmd_write(CMD_ADDRESS_VIN, load_voltage_mv);
            cmd_write(CMD_ADDRESS_ITOT, load_current_ma);

            vsen_sample_sum = 0;
            isen_sample_sum = 0;
            sample_count = 0;
        }

        kernel_sleep_ms(10);
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the VSEN ADC source (either VSEN_SRC_INTERNAL or VSEN_SRC_REMOTE)
void vi_sense_set_vsen_source(vsen_src_t source) {

    if (source == VSEN_SRC_INTERNAL) gpio_write(VSEN_SRC_GPIO, LOW);
    else if (source == VSEN_SRC_REMOTE) gpio_write(VSEN_SRC_GPIO, HIGH);
    else return;

    vsen_src = source;
}

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

// samples the load voltage using the external ADC
uint16_t __vsen_adc_read(void) {

    kernel_time_t start_time = kernel_get_time_ms();

    gpio_write(VSEN_ADC_SPI_SS_GPIO, LOW);
    spi_write(VSEN_ADC_SPI, 0x0000);
    
    while (!spi_tx_done(VSEN_ADC_SPI) && kernel_get_time_since(start_time) < 10);
    gpio_write(VSEN_ADC_SPI_SS_GPIO, HIGH);

    // pulse the !CONVST (TRIG) pin to trigger next conversion
    // no delay is needed, the pulse is about 60us, the datasheet minimum is 10us
    gpio_write(VSEN_ADC_CONVST_GPIO, LOW);
    gpio_write(VSEN_ADC_CONVST_GPIO, HIGH);

    return (spi_read(VSEN_ADC_SPI) >> 4);       // the ADC is 12 bit and the SPI is 16bit; shift the value by 4 bits to get the right alignment
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// samples the load current using the external ADC
uint16_t __isen_adc_read(void) {

    kernel_time_t start_time = kernel_get_time_ms();

    gpio_write(ISEN_ADC_SPI_SS_GPIO, LOW);
    spi_write(ISEN_ADC_SPI, 0x0000);

    while (!spi_tx_done(ISEN_ADC_SPI) && kernel_get_time_since(start_time) < 10);
    gpio_write(ISEN_ADC_SPI_SS_GPIO, HIGH);

    // pulse the !CONVST (TRIG) pin to trigger next conversion
    // no delay is needed, the pulse is about 60us, the datasheet minimum is 10us
    gpio_write(ISEN_ADC_CONVST_GPIO, LOW);
    gpio_write(ISEN_ADC_CONVST_GPIO, HIGH);

    return (spi_read(ISEN_ADC_SPI) >> 4);       // the ADC is 12 bit and the SPI is 16bit; shift the value by 4 bits to get the right alignment
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
