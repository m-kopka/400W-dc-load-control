#include "vi_sense.h"
#include "hal/spi.h"
#include "cmd_spi_driver.h"

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

void __read_latest_conversion(void);
void __read_latest_conversion_blocking(void);

void load_update_pid(uint32_t voltage, uint32_t current);

//---- INTERNAL DATA ---------------------------------------------------------------------------------------------------------------------------------------------

volatile bool conversion_read_started = false;      // ADC SPI is reading new data
bool continuous_conversion_mode_enabled = false;    // Continuous Conversion Mode is enabled (new conversion trigger and read is triggered immediately after reading the last one)
int32_t voltage_latest_sample_mv;                  // latest VSEN ADC conversion result converted to mV
int32_t current_latest_sample_ma;                  // latest ISEN ADC conversion result converted to mA
int32_t load_voltage_mv = 0;                       // current load voltage [mV]. Updated by the vi_sense_task (averaged)
int32_t load_current_ma = 0;                       // current load current [mA]. Updated by the vi_sense_task (averaged)
int32_t load_power_mw = 0;                         // current load power [mW]. Updated by the vi_sense_task (averaged)
uint32_t sink_current[4] = {0};                     // current of individual current sink [mA]
vsen_src_t vsen_src = VSEN_SRC_INTERNAL;            // voltage sense source (internal or remote)
bool auto_vsen_src_enabled = false;                 // automatic switching of voltage sense source enabled

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
    VSEN_ADC_SPI->CR2  = SPI_CR2_RXNEIE;    // enable the RX buffer not empty interrupt
    VSEN_ADC_SPI->CR1 |= SPI_CR1_SPE;       // spi enable

    // the SPI interrupt priority must be higher than the SVC interrupt priority so the SPI RXNEIE interrupt can preempt a kernel context switch
    NVIC_SetPriority(SVCall_IRQn, 1);
    NVIC_SetPriority(SPI5_IRQn, 0);
    NVIC_EnableIRQ(SPI5_IRQn);

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
    __read_latest_conversion_blocking();
    __read_latest_conversion_blocking();

    while (1) {

        static int32_t vsen_sample_sum = 0;     // sum of voltage samples
        static int32_t isen_sample_sum = 0;     // sum of current samples
        static int32_t power_sample_sum = 0;    // sum of previous power results
        static uint8_t sample_count = 0;        // number of samples contained in the vsen and isen sample_sum
        static uint8_t power_sample_count = 0;

        vsen_sample_sum += voltage_latest_sample_mv;
        isen_sample_sum += current_latest_sample_ma;
        sample_count++;

        if (sample_count == 8) {

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // divide sample sum by sample count
            int32_t vsen_new_value = vsen_sample_sum >> 3;
            int32_t isen_new_value = isen_sample_sum >> 3;

            // calculate a moving average for voltage
            if (load_voltage_mv > 0) load_voltage_mv = (load_voltage_mv + vsen_new_value) >> 1;
            else load_voltage_mv = vsen_new_value;

            // calculate a moving average for current
            if (load_current_ma > 0) load_current_ma = (load_current_ma + isen_new_value) >> 1;
            else load_current_ma = isen_new_value;

            // add power calculation to sample sum
            int32_t power_sample = load_voltage_mv * load_current_ma / 1000;
            power_sample_sum += power_sample;
            power_sample_count++;

            if (power_sample_count == 8) {

                // divide sample sum by sample count and calculate moving average
                load_power_mw = (load_power_mw + (power_sample_sum >> 3)) >> 1;

                if (load_current_ma > 5000) load_power_mw = (load_power_mw + 500) / 1000 * 1000;
                else load_power_mw = (load_power_mw + 250) / 500 * 500;

                power_sample_sum = 0;
                power_sample_count = 0;
            }

            // round to 100mV / 100mA
            if (load_current_ma > 5000) {
                
                load_voltage_mv = (load_voltage_mv + 50) / 100 * 100;
                load_current_ma = (load_current_ma + 50) / 100 * 100;

            } else {    // round to 50mV, 50mA
                
                load_voltage_mv = (load_voltage_mv + 25) / 50 * 50;
                load_current_ma = (load_current_ma + 25) / 50 * 50;
            }

            if (load_voltage_mv < VI_SENSE_MIN_VOLTAGE) {
                
                load_voltage_mv = 0;
                load_power_mw = 0;
            }

            if (load_current_ma < VI_SENSE_MIN_CURRENT){
                
                load_current_ma = 0;
                load_power_mw = 0;
            }

            // update registers
            cmd_write(CMD_ADDRESS_VOLTAGE, load_voltage_mv / 10);
            cmd_write(CMD_ADDRESS_CURRENT, load_current_ma);
            cmd_write(CMD_ADDRESS_POWER, load_power_mw / 100);

            vsen_sample_sum = 0;
            isen_sample_sum = 0;
            sample_count = 0;

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
            
            // handle automatic voltage sense source switching (not allowed in continuous conversion mode)
            if (auto_vsen_src_enabled && !continuous_conversion_mode_enabled) {

                // if the current VSEN source is internal and load voltage is not 0, try sampling voltage with remote sense
                // if the measured voltage is not zero, switch to remote sense
                if (vsen_src == VSEN_SRC_INTERNAL) {

                    if (load_voltage_mv > 0) {

                        // switch mux to remote sense and perform a dummy read to trigger a conversion with remote sense
                        gpio_write(VSEN_SRC_GPIO, HIGH);

                        // read in last conversion and trigger the next one, then read in a conversion with Remote Sense on
                        __read_latest_conversion_blocking();
                        __read_latest_conversion_blocking();

                        // if the measured code is above threshold, switch to remote sense
                        if (voltage_latest_sample_mv >= VI_SENSE_AUTO_VSENSRC_THRESHOLD_MV) vi_sense_set_vsen_source(VSEN_SRC_REMOTE);
                        else {

                            gpio_write(VSEN_SRC_GPIO, LOW);
                            __read_latest_conversion_blocking();
                        }
                    }

                // if the current VSEN source is remote and voltage is 0, switch to internal
                } else if (voltage_latest_sample_mv < VI_SENSE_AUTO_VSENSRC_THRESHOLD_MV) vi_sense_set_vsen_source(VSEN_SRC_INTERNAL);
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // get new samples
            if (!continuous_conversion_mode_enabled) {
                
                while (conversion_read_started) kernel_yield();
                __read_latest_conversion();
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // sample individual current sink voltage using the internal ADC
            for (int sink = CURRENT_L1; sink <= CURRENT_R2; sink++) {

                sink_current[sink] = internal_isen_read(sink);
            }

            cmd_write(CMD_ADDRESS_CURRENT_L1, sink_current[CURRENT_L1]);
            cmd_write(CMD_ADDRESS_CURRENT_L2, sink_current[CURRENT_L2]);
            cmd_write(CMD_ADDRESS_CURRENT_R1, sink_current[CURRENT_R1]);
            cmd_write(CMD_ADDRESS_CURRENT_R2, sink_current[CURRENT_R2]);

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
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

// enables or disables the automatic VSEN source switching feature
void vi_sense_set_automatic_vsen_source(bool enable) {

    auto_vsen_src_enabled = enable;

    if (enable) cmd_set_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_AUTO_VSEN_SRC);
    else cmd_clear_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_AUTO_VSEN_SRC);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// enables or disables the Continuous Conversion Mode (new conversion trigger and read is triggered immediately after reading the last one)
void vi_sense_set_continuous_conversion_mode(bool enabled) {

    continuous_conversion_mode_enabled = enabled;

    if (enabled) {
        
        vi_sense_set_vsen_source(VSEN_SRC_INTERNAL);        // Remote Sense is not allowed in Continuous Conversion Mode because the MUX switching would cause glitches in the digital feedback loop
        
        while(conversion_read_started) kernel_yield();
        __read_latest_conversion();                         // read last conversion (triggers the next one in Continuous Conversion Mode)
    }
}

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

// starts reading from the VSEN and ISEN ADCs simultaneously. The conversion_read_done flag will be raised after the read is complete.
// Results are then available in the voltage_latest_sample_mv and current_latest_sample_ma variables
void __read_latest_conversion(void) {

    conversion_read_started = true;     // set the flag first in case the program jumps to an ISR after the spi write and the read finishes before reaching the end of this function

    gpio_write(VSEN_ADC_SPI_SS_GPIO, LOW);
    gpio_write(ISEN_ADC_SPI_SS_GPIO, LOW);
    spi_write(VSEN_ADC_SPI, 0x0000);
    spi_write(ISEN_ADC_SPI, 0x0000);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// reads from the VSEN and ISEN ADCs simultaneously. Waits for the read to finish. Results are then available in the voltage_latest_sample_mv and current_latest_sample_ma variables
void __read_latest_conversion_blocking(void) {

    __read_latest_conversion();
    while (conversion_read_started) kernel_yield();
}

//---- IRQ HANDLERS ----------------------------------------------------------------------------------------------------------------------------------------------

// terminates SPI packet, triggers next conversion, stores data, initiates next conversion (for both ADCs simultaneously)
void VSEN_ADC_SPI_HANDLER() {

    if (spi_rx_not_empty(VSEN_ADC_SPI)) {

        // pull SS of both ADCs high
        gpio_write(VSEN_ADC_SPI_SS_GPIO, HIGH);
        gpio_write(ISEN_ADC_SPI_SS_GPIO, HIGH);

        // pulse the !CONVST (TRIG) pin to trigger next conversion
        gpio_write(VSEN_ADC_CONVST_GPIO, LOW);
        gpio_write(ISEN_ADC_CONVST_GPIO, LOW);
        gpio_write(VSEN_ADC_CONVST_GPIO, HIGH);
        gpio_write(ISEN_ADC_CONVST_GPIO, HIGH);

        // the ADC is 12 bit and the SPI is 16bit; shift the value by 4 bits to get the right alignment
        uint16_t voltage_code = (spi_read(VSEN_ADC_SPI) >> 4);
        uint16_t current_code = (spi_read(ISEN_ADC_SPI) >> 4);  

        voltage_latest_sample_mv = (vsen_src == VSEN_SRC_INTERNAL) ? VSEN_ADC_CODE_TO_MV_INT(voltage_code) : VSEN_ADC_CODE_TO_MV_REM(voltage_code);
        current_latest_sample_ma = ISEN_ADC_CODE_TO_MA(current_code);

        conversion_read_started = false;

        load_update_pid(voltage_latest_sample_mv, current_latest_sample_ma);

        if (continuous_conversion_mode_enabled) __read_latest_conversion();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
