#include "load_control.h"
#include "iset_dac.h"
#include "vi_sense.h"

extern load_mode_t load_mode;
extern bool enabled;
extern uint32_t cv_level_mv;
extern uint32_t cr_level_mr;
extern uint32_t cp_level_uw;  

int32_t cv_Kp = 50;
int32_t cv_Ki = 100;
int32_t cr_Kp = 50;
int32_t cr_Ki = 100;
int32_t cp_Kp = 60000;
int32_t cp_Ki = 250000;

int32_t integral = 0;

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

void __pid_reset(void) {

    integral = 0;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void load_update_pid(uint32_t voltage, uint32_t current) {

    gpio_write(ISET_DAC_SPI_SS_GPIO, HIGH);

    if (!enabled) return;

    int32_t error;
    int32_t output = 0;

    switch (load_mode) {

        //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

        case LOAD_MODE_CV: {

            error = voltage - cv_level_mv;

            int proportional = error / cv_Kp;
            integral += error / cv_Ki;

            if (integral > 100000) integral = 100000;
            if (integral < -100000) integral = -100000;

            output = ISET_DAC_ZERO_LEVEL_CODE - (proportional + integral);

        } break;

        //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

        case LOAD_MODE_CR: {

            uint32_t resistance = (voltage * 1000) / current;
            error = resistance - cr_level_mr;

            int32_t proportional = error / cr_Kp;
            integral += error / cr_Ki;

            if (integral > 100000) integral = 100000;
            if (integral < -100000) integral = -100000;

            output = ISET_DAC_ZERO_LEVEL_CODE - (proportional + integral);

        } break;

        //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

        case LOAD_MODE_CP: {

            uint32_t power = voltage * current;
            error = cp_level_uw - power;
                
            int32_t proportional = error / cp_Kp;
            integral += error / cp_Ki;

            if (integral > 100000) integral = 100000;
            if (integral < -100000) integral = -100000;

            output = ISET_DAC_ZERO_LEVEL_CODE - (proportional + integral);

        } break;

        //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

        default:
            return;
            break;
    }

    // clamp the PID output
    if (output < 0x0000) output = 0x0000;
    if (output > ISET_DAC_ZERO_LEVEL_CODE) output = ISET_DAC_ZERO_LEVEL_CODE;

    // update ISET_DAC
    iset_dac_write_code_non_blocking(output);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
