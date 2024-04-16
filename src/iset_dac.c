#include "iset_dac.h"
#include "hal/spi.h"
#include "hal/timer.h"

//---- INTERNAL DATA ---------------------------------------------------------------------------------------------------------------------------------------------

static volatile bool is_in_transient = false;   // ISET_DAC is in a slew limited transient
static volatile int32_t current_code = 0;       // most recent code sent to the DAC (used in slew limit logic)
static uint16_t target_code = 0;                // target DAC code in slew limited ramp

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

    iset_dac_write_code(0xffff);

    // setup the timer to trigger an interrupt in regular interval while the load current is in transition
    // set the timer frequency at 10x the required interrupt frequency and reload at 9 (interrupt every 10 counts)
    timer_init_counter(ISET_DAC_TIMER, ISET_DAC_TIMER_FREQUENCY * 10, TIMER_DIR_UP, 9);

    ISET_DAC_TIMER->CR1 |= TIM_CR1_URS;
    ISET_DAC_TIMER->EGR |= TIM_EGR_UG;
    ISET_DAC_TIMER->DIER |= TIM_DIER_UIE;
    ISET_DAC_TIMER->SR &= ~TIM_SR_UIF;

    NVIC_SetPriority(ISET_DAC_TIMER_IRQ, 1);
    NVIC_EnableIRQ(ISET_DAC_TIMER_IRQ);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the I_SET DAC output voltage to the corresponding current value
void iset_dac_set_current(uint32_t current_ma, bool slew_limit) {

    if (slew_limit) {       // change the DAC value in regular intervals until the target current is reached

        target_code = ISET_DAC_MA_TO_CODE(current_ma);
        is_in_transient = true;
        timer_start_count(ISET_DAC_TIMER);

    } else iset_dac_write_code(ISET_DAC_MA_TO_CODE(current_ma));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// writes the specified 16bit code to the ISET_DAC
void iset_dac_write_code(uint16_t code) {

    gpio_write(ISET_DAC_SPI_SS_GPIO, LOW);
    spi_write(ISET_DAC_SPI, code);

    while (!spi_tx_done(ISET_DAC_SPI));
    gpio_write(ISET_DAC_SPI_SS_GPIO, HIGH);

    current_code = code;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns true if the ISET_DAC is in a slew limited transient
bool iset_dac_is_in_transient(void) {

    return (is_in_transient);
}

//---- IRQ HANDLERS ----------------------------------------------------------------------------------------------------------------------------------------------

// triggered in regular intervals while the load current is in transient to slowly ramp the dac
void ISET_DAC_TIMER_IRQ_HANDLER(void) {

    if (bit_is_set(ISET_DAC_TIMER->SR, TIM_SR_UIF)) {

        if (current_code < target_code) {       // negative DAC ramp (positive current ramp)

            current_code -= ISET_DAC_LSB_PER_MA(SLEW_LIMIT_AMPS_PER_SECOND);
            if (current_code >= target_code) {      // target value reached, stop count
                
                current_code = target_code;
                is_in_transient = false;
                timer_stop_count(ISET_DAC_TIMER);
            }

        } else {                                // positive DAC ramp (negative current ramp)

            current_code += ISET_DAC_LSB_PER_MA(SLEW_LIMIT_AMPS_PER_SECOND);
            if (current_code <= target_code) {      // target value reached, stop count    
                
                current_code = target_code;
                is_in_transient = false;
                timer_stop_count(ISET_DAC_TIMER);
            }
        }

        iset_dac_write_code(current_code);
        clear_bits(ISET_DAC_TIMER->SR, TIM_SR_UIF);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
