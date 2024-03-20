#include "internal_isen.h"
#include "hal/adc.h"

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

static inline uint8_t get_adc_chan(internal_isen_t current_sink) {

         if (current_sink == CURRENT_L1) return (ISEN_L1_ADC_CH);
    else if (current_sink == CURRENT_L2) return (ISEN_L2_ADC_CH);
    else if (current_sink == CURRENT_R1) return (ISEN_R1_ADC_CH);
    else                                 return (ISEN_R2_ADC_CH);
}

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the ADC for internal current sensing
void internal_isen_init(void) {

    rcc_enable_peripheral_clock(ISEN_L1_GPIO_CLOCK);
    rcc_enable_peripheral_clock(ISEN_L2_GPIO_CLOCK);
    rcc_enable_peripheral_clock(ISEN_R1_GPIO_CLOCK);
    rcc_enable_peripheral_clock(ISEN_R2_GPIO_CLOCK);
    gpio_set_mode(ISEN_L1_GPIO, GPIO_MODE_ANALOG);
    gpio_set_mode(ISEN_L2_GPIO, GPIO_MODE_ANALOG);
    gpio_set_mode(ISEN_R1_GPIO, GPIO_MODE_ANALOG);
    gpio_set_mode(ISEN_R2_GPIO, GPIO_MODE_ANALOG);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// measures current of a individual current sink [mA]
uint16_t internal_isen_read(internal_isen_t current_sink) {

    if (current_sink != CURRENT_L1 && current_sink != CURRENT_L2 && current_sink != CURRENT_R1 && current_sink != CURRENT_R2) return 0;

    uint16_t current_ma = ISEN_INT_ADC_CODE_TO_MA(adc_read(get_adc_chan(current_sink)));
    return current_ma;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
