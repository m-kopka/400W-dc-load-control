#include "load_control.h"
#include "cmd_spi_driver.h"
#include "iset_dac.h"
#include "vi_sense.h"

//---- INTERNAL DATA ---------------------------------------------------------------------------------------------------------------------------------------------

extern load_mode_t load_mode;
extern bool enabled;                    // load is enabled (sinking current)
extern uint32_t cc_level_ma;            // current to be drawn in the CC mode when the load is enabled [mA]
extern uint32_t cv_level_mv;  
extern uint32_t cr_level_mr;  
extern uint32_t cp_level_uw;           
extern uint32_t discharge_voltage_mv;   // discharge voltage threshold; if the load voltage drops bellow this value, the load is automatically disabled [mV] (0 == feature is disabled)

// load registers
extern uint16_t status_register;        // load status flags
extern uint16_t fault_register;         // load fault flags
extern uint16_t fault_mask;             // fault mask; if the corresponding bit in the fault mask is 0, the fault flag is ignored

// statistics
extern kernel_time_t last_enable_time;  // absolute time of last load enable (not cleared after a load disable)
extern uint32_t total_mas;              // total cumulative current since the load was last enabled [mAs]
extern uint32_t total_mws;              // total power dissipated since the load was last enabled [mWs]

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

void ext_fault_task(void);

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
    load_set_mode(LOAD_MODE_CC);
    load_set_cc_level(LOAD_START_CC_LEVEL_MA);
    load_set_cv_level(LOAD_START_CV_LEVEL_MV);
    load_set_cr_level(LOAD_START_CR_LEVEL_MR);
    load_set_cp_level(LOAD_START_CP_LEVEL_MW);

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

            bool not_in_regulation = false;

            // check if the load is in regulation
            if (load_mode == LOAD_MODE_CC) {

                int32_t current_error = cc_level_ma - load_current_ma;
                if (current_error < 0) current_error = -current_error;
                
                // dac is not in transient and the current difference from setpoint is higher than the threshold
                if (!iset_dac_is_in_transient() && current_error >= LOAD_NO_REG_THRESHOLD_CC) not_in_regulation = true;

            } else if (load_mode == LOAD_MODE_CV) {

                int32_t error = cv_level_mv - load_voltage_mv;
                if (error < 0) error = -error;
                
                if (error >= LOAD_NO_REG_THRESHOLD_CV) not_in_regulation = true;

            } else if (load_mode == LOAD_MODE_CR) {

                uint32_t resistance = (load_voltage_mv * 1000) / load_current_ma;
                int32_t error = cr_level_mr - resistance;
                if (error < 0) error = -error;
                
                if (error >= LOAD_NO_REG_THRESHOLD_CR) not_in_regulation = true;

            } else {

                int32_t error = (cp_level_uw / 1000) - load_power_mw;
                if (error < 0) error = -error;
                
                if (error >= LOAD_NO_REG_THRESHOLD_CP) not_in_regulation = true;
            }
            
            static uint8_t no_reg_cumulative_counter = 0;

            if (not_in_regulation) {

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
            if (load_current_ma > LOAD_MIN_CC_LEVEL_MA && !iset_dac_is_in_transient()) {

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

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
