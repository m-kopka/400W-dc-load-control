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
bool auto_vsen_src_enabled = false;             // automatic switching of voltage sense source enabled

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
        uint16_t vsen_latest_sample = __vsen_adc_read();
        uint16_t isen_latest_sample = __isen_adc_read();
        vsen_sample_sum += vsen_latest_sample;
        isen_sample_sum += isen_latest_sample;
        sample_count++;

        if (sample_count == 8) {

            // divide sample sum by sample count and convert to mV and mA
            uint16_t vsen_new_value = (vsen_src == VSEN_SRC_INTERNAL) ? VSEN_ADC_CODE_TO_MV_INT(vsen_sample_sum >> 3) : VSEN_ADC_CODE_TO_MV_REM(vsen_sample_sum >> 3);
            uint16_t isen_new_value = ISEN_ADC_CODE_TO_MA(isen_sample_sum >> 3);

            // calculate a moving average for voltage
            if (load_voltage_mv > 0) load_voltage_mv = (load_voltage_mv + vsen_new_value) >> 1;
            else load_voltage_mv = vsen_new_value;

            // calculate a moving average for current
            if (load_current_ma > 0) load_current_ma = (load_current_ma + isen_new_value) >> 1;
            else load_current_ma = isen_new_value;

            // round to 50mV / 50mA
            load_voltage_mv = (load_voltage_mv + 25) / 50 * 50;
            load_current_ma = (load_current_ma + 25) / 50 * 50;

            if (load_voltage_mv < VI_SENSE_MIN_VOLTAGE) load_voltage_mv = 0;
            if (load_current_ma < VI_SENSE_MIN_CURRENT) load_current_ma = 0;

            // update registers
            cmd_write(CMD_ADDRESS_VOLTAGE, load_voltage_mv / 10);
            cmd_write(CMD_ADDRESS_CURRENT, load_current_ma);

            vsen_sample_sum = 0;
            isen_sample_sum = 0;
            sample_count = 0;

            // handle automatic voltage sense source switching
            if (auto_vsen_src_enabled) {

                // if the current VSEN source is internal and load voltage is not 0, try sampling voltage with remote sense
                // if the measured voltage is not zero, switch to remote sense
                if (vsen_src == VSEN_SRC_INTERNAL) {

                    if (load_voltage_mv > 0) {

                        // switch mux to remote sense and perform a dummy read to trigger a conversion with remote sense
                        gpio_write(VSEN_SRC_GPIO, HIGH);
                        __vsen_adc_read();

                        // wait for the conversion to finish
                        for (volatile int i = 0; i < 10; i++);

                        // if the measured code is above threshold, switch to remote sense
                        if (__vsen_adc_read() >= VI_SENSE_AUTO_VSENSRC_THRESHOLD_CODE) vi_sense_set_vsen_source(VSEN_SRC_REMOTE);
                        else {

                            gpio_write(VSEN_SRC_GPIO, LOW);
                            __vsen_adc_read();
                        }
                    }

                // if the current VSEN source is remote and voltage is 0, switch to internal
                } else if (vsen_latest_sample < VI_SENSE_AUTO_VSENSRC_THRESHOLD_CODE) vi_sense_set_vsen_source(VSEN_SRC_INTERNAL);
            }
        }

        kernel_sleep_ms(5);
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the VSEN ADC source (either VSEN_SRC_INTERNAL or VSEN_SRC_REMOTE)
void vi_sense_set_vsen_source(vsen_src_t source) {

    if (source == VSEN_SRC_INTERNAL) {
        
        gpio_write(VSEN_SRC_GPIO, LOW);
        cmd_clear_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_VSEN_SRC);
    
    } else if (source == VSEN_SRC_REMOTE) {
        
        gpio_write(VSEN_SRC_GPIO, HIGH);
        cmd_set_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_VSEN_SRC);

    } else return;

    vsen_src = source;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void vi_sense_set_automatic_vsen_source(bool enable) {

    auto_vsen_src_enabled = enable;

    if (enable) cmd_set_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_AUTO_VSEN_SRC);
    else cmd_clear_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_AUTO_VSEN_SRC);
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
