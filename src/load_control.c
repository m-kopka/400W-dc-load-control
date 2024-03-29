#include "load_control.h"
#include "cmd_spi_driver.h"
#include "iset_dac.h"
#include "vi_sense.h"

//---- INTERNAL DATA ---------------------------------------------------------------------------------------------------------------------------------------------

static bool enabled = false;        // load is enabled (sinking current)
uint16_t cc_level_ma = 0;           // current to be drawn in the CC mode when the load is enabled [mA]
uint32_t discharge_voltage_mv = 0;  // discharge voltage threshold; if the load voltage drops bellow this value, the load is automatically disabled [mV] (0 == feature is disabled)

// load registers
uint16_t status_register = 0;       // load status flags
uint16_t fault_register  = 0;       // load fault flags
uint16_t fault_mask      = 0;       // fault mask; if the corresponding bit in the fault mask is 0, the fault flag is ignored

// statistics
static kernel_time_t last_enable_time = 0;  // absolute time of last load enable (not cleared after a load disable)
static uint32_t total_mas = 0;              // total cumulative current since the load was last enabled [mAs]
static uint32_t total_mws = 0;              // total power dissipated since the load was last enabled [mWs]

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

void ext_fault_task(void);
void __check_fault_conditions(void);

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the load and handles various load functions on runtime
void load_control_task(void) {

    // LEDs
    rcc_enable_peripheral_clock(LOAD_ENABLE_LED_GPIO_CLOCK);
    rcc_enable_peripheral_clock(FAULT_LED_GPIO_CLOCK);
    gpio_write(LOAD_ENABLE_LED_GPIO, HIGH);
    gpio_write(FAULT_LED_GPIO, HIGH);
    gpio_set_mode(LOAD_ENABLE_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_mode(FAULT_LED_GPIO, GPIO_MODE_OUTPUT);

    // Power board enable pins
    rcc_enable_peripheral_clock(LOAD_EN_L_GPIO_CLOCK);
    rcc_enable_peripheral_clock(LOAD_EN_R_GPIO_CLOCK);
    gpio_set_mode(LOAD_EN_L_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_mode(LOAD_EN_R_GPIO, GPIO_MODE_OUTPUT);
    
    iset_dac_init();

    uint32_t vi_sense_stack[64];
    uint32_t ext_fault_stack[64];
    kernel_create_task(vi_sense_task, vi_sense_stack, sizeof(vi_sense_stack), 10);
    kernel_create_task(ext_fault_task, ext_fault_stack, sizeof(ext_fault_stack), 10);

    // wait for the CMD SPI interface to be initialized and set the default CC level and fault mask
    kernel_sleep_ms(100);
    load_set_fault_mask(LOAD_DEFAULT_FAULT_MASK);
    load_set_cc_level(LOAD_START_CC_LEVEL_MA);
    cmd_write(CMD_ADDRESS_AVLBL_CURRENT, LOAD_AVAILABLE_CURRENT_A);
    cmd_write(CMD_ADDRESS_AVLBL_POWER, LOAD_AVAILABLE_POWER_W);

    while (1) {

        if (enabled) {

            uint32_t load_voltage_mv = vi_sense_get_voltage();
            uint32_t load_current_ma = vi_sense_get_current();
            uint32_t load_power_mw = load_voltage_mv * load_current_ma / 1000;

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // overcurrent protection
            if (load_current_ma > LOAD_OCP_THRESHOLD_MA) load_trigger_fault(LOAD_FAULT_OCP);

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // overpower protection
            if (load_power_mw > LOAD_OPP_THRESHOLD_MW) load_trigger_fault(LOAD_FAULT_OPP);

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // check if the load is in regulation
            int32_t current_error = cc_level_ma - load_current_ma;
            if (current_error < 0) current_error = -current_error;

            static uint8_t no_reg_cumulative_counter = 0;

            // dac is not in transient and the current difference from setpoint is higher than the threshold
            if (!iset_dac_is_in_transient() && current_error >= LOAD_NO_REG_THRESHOLD) {

                // raise NO_REG flag after enough cumulative faults
                if (++no_reg_cumulative_counter == LOAD_NO_REG_CUMULATIVE_COUNTS) {

                    status_register |= LOAD_STATUS_NO_REG;
                    cmd_write(CMD_ADDRESS_STATUS, status_register);

                    load_trigger_fault(LOAD_FAULT_REG);

                    no_reg_cumulative_counter = LOAD_NO_REG_CUMULATIVE_COUNTS - 1;
                }

            // load is in regulation, clear the NO_REG flag
            } else {
                
                status_register &= ~LOAD_STATUS_NO_REG;
                cmd_write(CMD_ADDRESS_STATUS, status_register);

                if (no_reg_cumulative_counter > 0) no_reg_cumulative_counter--;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // check for fuse faults
            if (load_current_ma > LOAD_MIN_CURRENT_MA && !iset_dac_is_in_transient()) {

                static uint8_t fuse_fault_cumulative_counter[4] = {0};

                // check the current of each current sink
                for (int sink = CURRENT_L1; sink <= CURRENT_R2; sink++) {

                    // trigger a fuse fault after enough cumulative faults
                    if (vi_sense_get_sink_current(sink) == 0) {
                        
                        if (++fuse_fault_cumulative_counter[sink] == LOAD_FUSE_FAULT_CUMULATIVE_COUNTS) {

                            if      (sink == CURRENT_L1) load_trigger_fault(LOAD_FAULT_FUSE_L1);
                            else if (sink == CURRENT_L2) load_trigger_fault(LOAD_FAULT_FUSE_L2);
                            else if (sink == CURRENT_R1) load_trigger_fault(LOAD_FAULT_FUSE_R1);
                            else if (sink == CURRENT_R2) load_trigger_fault(LOAD_FAULT_FUSE_R2);

                            fuse_fault_cumulative_counter[sink] = LOAD_FUSE_FAULT_CUMULATIVE_COUNTS - 1;
                        }

                    } else if (fuse_fault_cumulative_counter[sink] > 0) fuse_fault_cumulative_counter[sink]--;
                }
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // disable the load if voltage dropped bellow threshold
            if (load_voltage_mv < discharge_voltage_mv) load_set_enable(false);

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // handle load statistics
            uint32_t enable_time_s = kernel_get_time_since(last_enable_time) / 1000;

            total_mas += load_current_ma / (1000 / LOAD_CONTROL_UPDATE_PERIOD_MS);
            total_mws += load_power_mw / (1000 / LOAD_CONTROL_UPDATE_PERIOD_MS);

            cmd_write(CMD_ADDRESS_TOTAL_TIME_L, enable_time_s & 0xffff);
            cmd_write(CMD_ADDRESS_TOTAL_TIME_H, enable_time_s >> 16);

            uint32_t total_mah = total_mas / (60 * 60);
            cmd_write(CMD_ADDRESS_TOTAL_MAH_L, total_mah & 0xffff);
            cmd_write(CMD_ADDRESS_TOTAL_MAH_H, total_mah >> 16);

            uint32_t total_mwh = total_mws / (60 * 60);
            cmd_write(CMD_ADDRESS_TOTAL_MWH_L, total_mwh & 0xffff);
            cmd_write(CMD_ADDRESS_TOTAL_MWH_H, total_mwh >> 16);

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        }

        kernel_sleep_ms(LOAD_CONTROL_UPDATE_PERIOD_MS);
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

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
        iset_dac_set_current(cc_level_ma, true);
        gpio_write(LOAD_ENABLE_LED_GPIO, LOW);

        last_enable_time = kernel_get_time_ms();
        total_mas = 0;
        total_mws = 0;

        status_register |=  LOAD_STATUS_ENABLED;

    } else {    // disable the load

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

// sets the load current in Constant Current mode
void load_set_cc_level(uint16_t current_ma) {

    // check limits
    if (current_ma < LOAD_MIN_CURRENT_MA) current_ma = LOAD_MIN_CURRENT_MA;
    if (current_ma > LOAD_MAX_CURRENT_MA) current_ma = LOAD_MAX_CURRENT_MA;

    if (enabled) iset_dac_set_current(current_ma, true);    // if the load is enabled write new value to the I_SET dac with a slew rate limit

    cc_level_ma = current_ma;
    cmd_write(CMD_ADDRESS_CC_LEVEL, current_ma);        // update the register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the discharge threshold voltage; if the load voltage drops bellow this value, the load is automatically disabled
void load_set_discharge_voltage(uint32_t voltage_mv) {

    discharge_voltage_mv = voltage_mv;
    cmd_write(CMD_ADDRESS_DISCH_LEVEL, voltage_mv);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the specified fault in the fault register and puts the load in a fault state if the fault is masked
void load_trigger_fault(load_fault_t fault) {

    uint16_t old_fault_register = fault_register;
    fault_register |= fault;
    if (old_fault_register == fault_register) return;   // fault already triggered, skip

    __check_fault_conditions();     // test fault status with fault mask and disable the load if the fault conditions are met
    
    cmd_write(CMD_ADDRESS_FAULT, fault_register);       // update the fault register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// clears a load fault flag
void load_clear_fault(load_fault_t fault) {

    fault_register &= ~fault;
    __check_fault_conditions();     // test fault status with fault mask and disable the load if the fault conditions are met

    cmd_write(CMD_ADDRESS_FAULT, fault_register);       // update the fault register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the load fault mask
void load_set_fault_mask(load_fault_t mask) {

    fault_mask = mask | LOAD_NON_MASKABLE_FAULTS;       // don't allow the always masked faults to be cleared
    __check_fault_conditions();                         // test fault status with fault mask and disable the load if the fault conditions are met

    cmd_write(CMD_ADDRESS_FAULT_MASK, fault_mask);       // update the fault mask register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the ready flag in the status register
void load_set_ready(bool ready) {

    if (ready) status_register |= LOAD_STATUS_READY;
    else status_register &= ~LOAD_STATUS_READY;

    cmd_write(CMD_ADDRESS_STATUS, status_register);
}

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

// checks if the current state of the fault register and fault mask should cause a load fault state
// if yes, it disables the load and updates the fault state and status register
// called after fault register of fault mask is updated
void __check_fault_conditions(void) {

    // if any of the fault flags are not masked
    if (fault_register & fault_mask) {

        load_set_enable(false);

        // external fault doesn't turn on the FAULT LED and doesn't cause the module to pull down its FAULT pin
        if (fault_register & ~LOAD_FAULT_EXTERNAL) {

            gpio_write(EXT_FAULT_GPIO, LOW);
            gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_OUTPUT);
            gpio_write(FAULT_LED_GPIO, LOW);

        } else {    // only external fault is triggered

            gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_INPUT);
            gpio_write(FAULT_LED_GPIO, HIGH);
        }

        status_register |= LOAD_STATUS_FAULT;       // set the fault bit in the status register

    } else {    // unmasked faults are triggered

        gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_INPUT);
        gpio_write(FAULT_LED_GPIO, HIGH);

        status_register &= ~LOAD_STATUS_FAULT;   // clear the fault bit in the status register
    }
    
    cmd_write(CMD_ADDRESS_STATUS, status_register);     // update the status register
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// debounces the external fault pin and triggers EXT faults
void ext_fault_task(void) {

    rcc_enable_peripheral_clock(EXT_FAULT_GPIO_CLOCK);
    gpio_set_mode(EXT_FAULT_GPIO, GPIO_MODE_INPUT);

    while (1) {

        static uint8_t debounce_counter = 0xff;
        static bool triggered = false;

        debounce_counter = (debounce_counter << 1) | gpio_get(EXT_FAULT_GPIO);

        if (debounce_counter == 0x00) {

            if (!triggered && !(fault_register & fault_mask)) {

                load_trigger_fault(LOAD_FAULT_EXTERNAL);
                triggered = true;
            }

        } else if (debounce_counter == 0xff) triggered = false;

        kernel_yield();
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
