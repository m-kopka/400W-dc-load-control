#include "temp_sensors.h"

//---- PRIVATE DATA ----------------------------------------------------------------------------------------------------------------------------------------------

// lookup table for conversion from raw 8bit adc value to temperature in °C
const uint8_t temp_sensor_lut[256] = {

    255, 255, 255, 255, 255, 255, 244, 233, 225, 217, 211, 205, 200, 195, 191, 187,
    183, 180, 177, 174, 171, 169, 166, 164, 162, 159, 157, 155, 154, 152, 150, 148,
    147, 145, 144, 142, 141, 140, 138, 137, 136, 134, 133, 132, 131, 130, 129, 128,
    127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117, 117, 116, 115, 114, 113,
    113, 112, 111, 110, 110, 109, 108, 107, 107, 106, 105, 105, 104, 103, 103, 102,
    101, 101, 100, 100,  99,  98,  98,  97,  97,  96,  95,  95,  94,  94,  93,  93,
    92,   91,  91,  90,  90,  89,  89,  88,  88,  87,  87,  86,  86,  85,  85,  84,
    84,   83,  83,  82,  82,  81,  81,  80,  80,  79,  79,  78,  78,  77,  77,  76,
    76,   76,  75,  75,  74,  74,  73,  73,  72,  72,  71,  71,  71,  70,  70,  69,
    69,   68,  68,  67,  67,  66,  66,  66,  65,  65,  64,  64,  63,  63,  62,  62,
    62,   61,  61,  60,  60,  59,  59,  58,  58,  57,  57,  57,  56,  56,  55,  55,
    54,   54,  53,  53,  52,  52,  51,  51,  51,  50,  50,  49,  49,  48,  48,  47,
    47,   46,  46,  45,  45,  44,  44,  43,  43,  42,  42,  41,  41,  40,  39,  39,
    38,   38,  37,  37,  36,  35,  35,  34,  34,  33,  32,  32,  31,  30,  30,  29,
    28,   28,  27,  26,  26,  25,  24,  23,  23,  22,  21,  20,  19,  18,  17,  16,
    16,   15,  13,  12,  11,  10,   9,   8,   6,   5,   3,   2,   0,   0,   0,   0
};

static temp_sensor_fault_t fault_flags[2] = {TEMP_SEN_FAULT_NONE};      // stores faults after a temp_sensor_read() function call

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the ADC for power transistor temperature sensing
void temp_sensor_init(void) {

    rcc_enable_peripheral_clock(TEMP_SEN_L_GPIO_CLOCK);
    rcc_enable_peripheral_clock(TEMP_SEN_R_GPIO_CLOCK);
    gpio_set_mode(TEMP_SEN_L_GPIO, GPIO_MODE_ANALOG);
    gpio_set_mode(TEMP_SEN_R_GPIO, GPIO_MODE_ANALOG);
    adc_init();
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// measures a power transistor temperature and returns it [°C]. Returns 255 in case of a fault
uint8_t temp_sensor_read(temp_sensor_t sensor) {

    if (sensor != TEMP_L && sensor != TEMP_R) return 255;

    for (int attempt = 0; attempt < TEMP_SEN_READ_ATTEMPTS; attempt++) {

        uint16_t raw_val = adc_read((sensor == TEMP_L) ? TEMP_SEN_L_ADC_CH : TEMP_SEN_R_ADC_CH);

        if (raw_val > 4095 - TEMP_SEN_OPEN_THRESHOLD) fault_flags[sensor] = TEMP_SEN_FAULT_OPEN;
        else if (raw_val <  TEMP_SEN_SHORT_THRESHOLD) fault_flags[sensor] = TEMP_SEN_FAULT_SHORT;
        else {

            fault_flags[sensor] = TEMP_SEN_FAULT_NONE;
            return (temp_sensor_lut[raw_val >> 4]);
        }
    }

    return 255;       // measurement failed, return 255°C in case the callee doesn't read the fault flag
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the raw 12bit ADC reading of a power transistor temperature (for calibration)
uint16_t temp_sensor_read_raw(temp_sensor_t sensor) {

    if (sensor != TEMP_L && sensor != TEMP_R) return 0;

    return (adc_read((sensor == TEMP_L) ? TEMP_SEN_L_ADC_CH : TEMP_SEN_R_ADC_CH));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the temperature sensor's fault flags
temp_sensor_fault_t temp_sensor_read_faults(temp_sensor_t sensor) {

    if (sensor != TEMP_L && sensor != TEMP_R) return TEMP_SEN_FAULT_NONE;
    return fault_flags[sensor];
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
