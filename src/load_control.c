#include "load_control.h"
#include "cmd_spi_driver.h"
#include "hal/timer.h"
#include "hal/spi.h"
#include "iset_dac.h"
#include "vi_sense.h"

//---- INTERNAL DATA ---------------------------------------------------------------------------------------------------------------------------------------------

load_mode_t load_mode = LOAD_MODE_CC;
bool enabled = false;               // load is enabled (sinking current)
uint32_t cc_level_ma = 0;           // current to be drawn in the CC mode when the load is enabled [mA]
uint32_t cv_level_mv = 0;
uint32_t cr_level_mr  = 0;
uint32_t cp_level_uw = 0;           
uint32_t discharge_voltage_mv = 0;  // discharge voltage threshold; if the load voltage drops bellow this value, the load is automatically disabled [mV] (0 == feature is disabled)

// load registers
uint16_t status_register = 0;       // load status flags
uint16_t fault_register  = 0;       // load fault flags
uint16_t fault_mask      = 0;       // fault mask; if the corresponding bit in the fault mask is 0, the fault flag is ignored

// statistics
kernel_time_t last_enable_time = 0;  // absolute time of last load enable (not cleared after a load disable)
uint32_t total_mas = 0;              // total cumulative current since the load was last enabled [mAs]
uint32_t total_mws = 0;              // total power dissipated since the load was last enabled [mWs]

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

void __pid_reset(void);

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// enables or disables the load; returns true if the action was successful; returns false if the load is in a fault state or not ready
bool load_set_enable(bool state) {

    if (state == enabled) return true;                                  // load is already in the specified state
    if (state && !(status_register & LOAD_STATUS_READY)) return false;  // load is performing a self test (not ready)
    if (state && (fault_register & fault_mask)) return false;           // load is in fault; enable not allowed until all masked faults are cleared

    if (state) {    // enable the load

        // set the minimum current level, enable the power boards and slowly increase the current to CC level
        iset_dac_set_current(LOAD_MIN_CURRENT_MA, false);
        gpio_write(LOAD_EN_L_GPIO, HIGH);
        gpio_write(LOAD_EN_R_GPIO, HIGH);

        if (load_mode == LOAD_MODE_CC) iset_dac_set_current(cc_level_ma, true);
        else {

            // setup the PID for CV, CR or CP
            __pid_reset();
            vi_sense_set_continuous_conversion_mode(true);
        }

        gpio_write(LOAD_ENABLE_LED_GPIO, LOW);

        last_enable_time = kernel_get_time_ms();
        total_mas = 0;
        total_mws = 0;

        status_register |=  LOAD_STATUS_ENABLED;

    } else {    // disable the load

        vi_sense_set_continuous_conversion_mode(false);

        // disable the power boards and write zero-current code to I_SET DAC
        gpio_write(LOAD_EN_L_GPIO, LOW);
        gpio_write(LOAD_EN_R_GPIO, LOW);
        iset_dac_write_code(0xffff);
        gpio_write(LOAD_ENABLE_LED_GPIO, HIGH);

        status_register &= ~(LOAD_STATUS_ENABLED | LOAD_STATUS_NO_REG);
    }

    enabled = state;
    cmd_write(CMD_ADDRESS_STATUS, status_register);     // update the status register

    return true;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the load mode (CC, CV, CR or CP)
void load_set_mode(load_mode_t mode) {

    if (mode == load_mode) return;
    if (mode != LOAD_MODE_CC && mode != LOAD_MODE_CV && mode != LOAD_MODE_CR && mode != LOAD_MODE_CP) return;

    load_set_enable(false);

    load_mode = mode;

    // update the config register
    if (mode & (1 << 0)) cmd_set_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_MODE0);
    else cmd_clear_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_MODE0);
    if (mode & (1 << 1)) cmd_set_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_MODE1);
    else cmd_clear_bit(CMD_ADDRESS_CONFIG, LOAD_CONFIG_MODE1);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the load current in Constant Current mode
void load_set_cc_level(uint32_t current_ma) {

    // check limits
    if (current_ma < LOAD_MIN_CURRENT_MA) current_ma = LOAD_MIN_CURRENT_MA;
    if (current_ma > LOAD_MAX_CURRENT_MA) current_ma = LOAD_MAX_CURRENT_MA;

    if (enabled) iset_dac_set_current(current_ma, true);    // if the load is enabled write new value to the I_SET dac with a slew rate limit

    cc_level_ma = current_ma;
    cmd_write(CMD_ADDRESS_CC_LEVEL, current_ma);        // update the register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the load voltage in Constant Voltage mode
void load_set_cv_level(uint32_t voltage_mv) {

    // check limits
    if (voltage_mv < 1000) voltage_mv = 1000;
    if (voltage_mv > 80000) voltage_mv = 800000;

    cv_level_mv = voltage_mv;
    cmd_write(CMD_ADDRESS_CV_LEVEL, voltage_mv / 10);        // update the register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the load resistance in Constant Resistance mode
void load_set_cr_level(uint32_t resistance_mohm) {

    // check limits
    if (resistance_mohm < 1) resistance_mohm = 1;
    if (resistance_mohm > 1000000) resistance_mohm = 1000000;

    cr_level_mr = resistance_mohm;
    cmd_write(CMD_ADDRESS_CR_LEVEL, resistance_mohm);        // update the register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the load power in Constant Power mode
void load_set_cp_level(uint32_t power_mw) {

    // check limits
    if (power_mw < 1000) power_mw = 1000;
    if (power_mw > 400000) power_mw = 400000;

    cp_level_uw = power_mw * 1000;
    cmd_write(CMD_ADDRESS_CP_LEVEL, power_mw / 100);        // update the register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the discharge threshold voltage; if the load voltage drops bellow this value, the load is automatically disabled
void load_set_discharge_voltage(uint32_t voltage_mv) {

    discharge_voltage_mv = voltage_mv;
    cmd_write(CMD_ADDRESS_DISCH_LEVEL, voltage_mv);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the ready flag in the status register
void load_set_ready(bool ready) {

    if (ready) status_register |= LOAD_STATUS_READY;
    else status_register &= ~LOAD_STATUS_READY;

    cmd_write(CMD_ADDRESS_STATUS, status_register);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
