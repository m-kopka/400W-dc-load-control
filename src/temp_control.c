#include "temp_control.h"
#include "load_control.h"
#include "cmd_interface/cmd_spi_driver.h"

typedef enum {

    CONTROL_STATE_IDLE,             // fan control is inactive, temperature too low
    CONTROL_STATE_IN_REGULATION,    // fan control is active
    CONTROL_STATE_STABLE,           // temperature stabilized and fans are kept at constant speed
    CONTROL_STATE_IN_OTP,           // overtemperature protection triggered
    CONTROL_STATE_IN_FAULT          // sensor or fan fault

} temp_control_state_t;

temp_control_state_t control_state = CONTROL_STATE_IDLE;
bool selfest_done = false;
uint16_t fault_flags = 0x0000;
uint32_t sensor_fault_cumulative_counter[2] = {0};
uint32_t fan_fault_cumulative_counter[2] = {0};

void __trigger_otp(void);

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// regulates fan speed based on power transistor temperature
void temp_control_task(void) {

    // start the fan regulator task as a child of this task
    uint32_t fan_regulator_stack[64];
    kernel_create_task(fan_regulator_task, fan_regulator_stack, sizeof(fan_regulator_stack), 1000);

    temp_sensor_init();

    fan_set_pwm(FAN_IDLE_PWM);

    while (1) {

        //---- TEMPERATURE MEASUREMENT ---------------------------------------------------------------------------------------------------------------------------
        
        uint16_t temperature_l = temp_sensor_read_fixed(TEMP_L);
        uint16_t temperature_r = temp_sensor_read_fixed(TEMP_R);

        cmd_write(CMD_ADDRESS_TEMP_L, temp_sensor_q8_2_to_int(temperature_l));
        cmd_write(CMD_ADDRESS_TEMP_R, temp_sensor_q8_2_to_int(temperature_r));

        //---- HANDLING SENSOR FAULTS ----------------------------------------------------------------------------------------------------------------------------

        for (int sensor = TEMP_L; sensor <= TEMP_R; sensor++) {

            temp_sensor_fault_t sensor_fault = temp_sensor_read_faults(sensor);

            // fault is short or open
            if (sensor_fault != TEMP_SEN_FAULT_NONE) {

                // trigger a sensor fault after after 4 cumulative faults
                if (++sensor_fault_cumulative_counter[sensor] == 4) {

                    write_masked(fault_flags, sensor_fault << (sensor * 2), 0x3 << (sensor * 2));
                    control_state = CONTROL_STATE_IN_FAULT;
                    fan_set_pwm(0);
                    trigger_fault(LOAD_FAULT_TEMP_SENSOR);

                    sensor_fault_cumulative_counter[sensor] = 3;
                }

            // no fault, decrement the cumulative counter
            } else if (sensor_fault_cumulative_counter[sensor] > 0) sensor_fault_cumulative_counter[sensor]--;
        }

        //---- HANDLING FAN FAULTS -------------------------------------------------------------------------------------------------------------------------------

        uint8_t fan_pwm = fan_get_pwm();
        
        // check the RPM of both fans
        for (int fan = FAN1; fan <= FAN2; fan++) {

            uint16_t fan_rpm = fan_get_rpm(fan);

            // fan PWM is above idle but no spin
            if (fan_pwm > 30 && fan_rpm == 0) {

                if (++fan_fault_cumulative_counter[fan] == 8) {

                    set_bits(fault_flags, 1 << (4 + fan));
                    control_state = CONTROL_STATE_IN_FAULT;
                    fan_set_pwm(0);
                    trigger_fault(LOAD_FAULT_FAN);

                    fan_fault_cumulative_counter[fan] = 7; 
                }

            } else if (fan_fault_cumulative_counter > 0) fan_fault_cumulative_counter[fan]--;

            cmd_write((fan == FAN1) ? CMD_ADDRESS_RPM1 : CMD_ADDRESS_RPM2, fan_pwm);
        }

        //---- FAN CONTROL ---------------------------------------------------------------------------------------------------------------------------------------

        uint16_t temperature = (temperature_l > temperature_r) ? temperature_l : temperature_r;     // regulate accordung to the highest measured temperature
        static uint16_t stable_temperature = 0;         // set to current temperature when the fan pwm stabilizes

        switch (control_state) {

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // temperature is low, fans idle
            case CONTROL_STATE_IDLE: {

                // temperature above threshold, start ramping up the fans
                if (temperature > temp_sensor_int_to_q8_2(TEMP_REGULATION_START_TEMP)) control_state = CONTROL_STATE_IN_REGULATION;
                break;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // fans are ramping up or down
            case CONTROL_STATE_IN_REGULATION: {

                static uint16_t stable_pwm_count = 0;
                uint8_t prev_pwm = fan_get_pwm();

                int16_t pwm = TEMP_REGULATION_SLOPE * (temperature - temp_sensor_int_to_q8_2(TEMP_REGULATION_START_TEMP)) + FAN_MIN_PWM;

                if      (pwm < FAN_MIN_PWM) pwm = FAN_MIN_PWM;
                else if (pwm > FAN_MAX_PWM) pwm = FAN_MAX_PWM;

                fan_set_pwm(pwm);

                if (pwm == prev_pwm) {

                    // pwm was stable for 16 iterations, temperature is stable
                    if (++stable_pwm_count == 16) {

                        stable_temperature = temperature;
                        control_state = CONTROL_STATE_STABLE;
                    }

                }   else stable_pwm_count = 0;

                if (temperature > temp_sensor_int_to_q8_2(TEMP_REGULATION_OTP_START_TEMP)) __trigger_otp();
                else if (temperature < temp_sensor_int_to_q8_2(TEMP_REGULATION_STOP_TEMP)) {
                    
                    fan_set_pwm(FAN_IDLE_PWM);
                    control_state = CONTROL_STATE_IDLE;     // transistors cooled down - go back to idle
                }

                break;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // temperature stabilized and fans are kept at constant speed
            case CONTROL_STATE_STABLE: {

                // temperature changed by 1 °C, go back to fan regulation
                     if (temperature >= stable_temperature + temp_sensor_int_to_q8_2(1)) control_state = CONTROL_STATE_IN_REGULATION;
                else if (temperature <= stable_temperature - temp_sensor_int_to_q8_2(1)) control_state = CONTROL_STATE_IN_REGULATION;

                break;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // overtemperature protection triggered
            case CONTROL_STATE_IN_OTP: {

                // load cooled down after an OTP trigger, stop the fans; the OTP fault remains triggered until clearing by master
                if (temperature < temp_sensor_int_to_q8_2(TEMP_REGULATION_OTP_STOP_TEMP)) control_state = CONTROL_STATE_IN_REGULATION;
                break;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            case CONTROL_STATE_IN_FAULT: {

                fan_set_pwm(0);
                break;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        }

        //--------------------------------------------------------------------------------------------------------------------------------------------------------

        kernel_sleep_ms(500);
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void __trigger_otp(void) {

    set_bits(fault_flags, 1 << (6 + 0));
    trigger_fault(LOAD_FAULT_OTP);

    control_state = CONTROL_STATE_IN_OTP;
    fan_set_pwm(FAN_MAX_PWM);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

void temp_control_clear_faults(void) {

    fault_flags = 0x00;
    sensor_fault_cumulative_counter[0] = 0;
    sensor_fault_cumulative_counter[1] = 0;
    fan_fault_cumulative_counter[0] = 0;
    fan_fault_cumulative_counter[1] = 0;

    if (control_state == CONTROL_STATE_IN_FAULT) control_state = CONTROL_STATE_IDLE;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
