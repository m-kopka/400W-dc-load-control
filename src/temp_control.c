#include "temp_control.h"
#include "load_control.h"
#include "cmd_interface/cmd_spi_driver.h"

//---- ENUMERATIONS ----------------------------------------------------------------------------------------------------------------------------------------------

typedef enum {

    CONTROL_STATE_IDLE,             // fan control is inactive, temperature too low
    CONTROL_STATE_IN_REGULATION,    // fan control is active
    CONTROL_STATE_STABLE,           // temperature stabilized and fans are kept at constant speed
    CONTROL_STATE_IN_OTP,           // overtemperature protection triggered

} temp_control_state_t;

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// regulates fan speed based on power transistor temperature
void temp_control_task(void) {

    // start the fan regulator task as a child of this task
    uint32_t fan_regulator_stack[64];
    kernel_create_task(fan_regulator_task, fan_regulator_stack, sizeof(fan_regulator_stack), 1000);

    temp_sensor_init();

    //---- FAN TEST ----------------------------------------------------------------------------------------------------------------------------------------------

#if FAN_TEST_ENABLED

    fan_set_pwm(FAN_TEST_SPEED);
    kernel_sleep_ms(FAN_TEST_DURATION);

    for (int fan = FAN1; fan <= FAN2; fan++) {

        if (fan_get_rpm(fan) < FAN_TEST_FAIL_RPM) load_trigger_fault((fan == FAN1) ? LOAD_FAULT_FAN1 : LOAD_FAULT_FAN2);
    }

#endif

    fan_set_pwm(FAN_IDLE_PWM);

    //---- TEMPERATURE SENSOR TEST -------------------------------------------------------------------------------------------------------------------------------

    for (int sensor = TEMP_L; sensor <= TEMP_R; sensor++) {

        int success_count = 0;

        for (int i = 0; i < 4; i++) {

            temp_sensor_read(sensor);
            if (temp_sensor_read_faults(sensor) == TEMP_SEN_FAULT_NONE) success_count++;
        }

        if (success_count == 0) load_trigger_fault((sensor == TEMP_L) ? LOAD_FAULT_TEMP_L : LOAD_FAULT_TEMP_R);
    }

    //------------------------------------------------------------------------------------------------------------------------------------------------------------

    load_set_ready(true);       // inform the control logic that the load is ready

    while (1) {

        //---- TEMPERATURE MEASUREMENT ---------------------------------------------------------------------------------------------------------------------------
        
        uint16_t temperature[2];
        temperature[TEMP_L] = temp_sensor_read_fixed(TEMP_L);
        temperature[TEMP_R] = temp_sensor_read_fixed(TEMP_R);

        //---- HANDLING SENSOR FAULTS ----------------------------------------------------------------------------------------------------------------------------

        static uint32_t sensor_fault_cumulative_counter[2] = {0};

        for (int sensor = TEMP_L; sensor <= TEMP_R; sensor++) {

            temp_sensor_fault_t sensor_fault = temp_sensor_read_faults(sensor);

            // fault is short or open
            if (sensor_fault != TEMP_SEN_FAULT_NONE) {

                temperature[sensor] = 0;

                // trigger a sensor fault after after enough cumulative faults
                if (++sensor_fault_cumulative_counter[sensor] == TEMP_SENSOR_FAULT_CUMULATIVE_THRESHOLD) {

                    load_trigger_fault((sensor == TEMP_L) ? LOAD_FAULT_TEMP_L : LOAD_FAULT_TEMP_R);
                    sensor_fault_cumulative_counter[sensor] = TEMP_SENSOR_FAULT_CUMULATIVE_THRESHOLD - 1;
                }

            // no fault, decrement the cumulative counter
            } else if (sensor_fault_cumulative_counter[sensor] > 0) sensor_fault_cumulative_counter[sensor]--;
        }

        //---- HANDLING FAN FAULTS -------------------------------------------------------------------------------------------------------------------------------

        static uint32_t fan_fault_cumulative_counter[2] = {0};
        uint8_t fan_pwm = fan_get_pwm();
        
        // check the RPM of both fans
        for (int fan = FAN1; fan <= FAN2; fan++) {

            uint16_t fan_rpm = fan_get_rpm(fan);

            // fan PWM is above idle but no spin
            if (fan_pwm > FAN_MIN_PWM && fan_rpm == 0) {

                // trigger a fan fault after after enough cumulative faults
                if (++fan_fault_cumulative_counter[fan] == FAN_FAULT_CUMULATIVE_THRESHOLD) {

                    load_trigger_fault((fan == FAN1) ? LOAD_FAULT_FAN1 : LOAD_FAULT_FAN2);
                    fan_fault_cumulative_counter[fan] = FAN_FAULT_CUMULATIVE_THRESHOLD - 1; 
                }

            } else if (fan_fault_cumulative_counter[fan] > 0) fan_fault_cumulative_counter[fan]--;

            cmd_write((fan == FAN1) ? CMD_ADDRESS_FAN_RPM1 : CMD_ADDRESS_FAN_RPM2, fan_rpm);
        }

        //---- FAN CONTROL ---------------------------------------------------------------------------------------------------------------------------------------

        static temp_control_state_t control_state = CONTROL_STATE_IDLE;     // temperature control state machine
        uint16_t max_temperature = (temperature[TEMP_L] > temperature[TEMP_R]) ? temperature[TEMP_L] : temperature[TEMP_R];     // regulate according to the highest measured temperature
        static uint16_t stable_temperature = 0;         // set to current temperature when the fan pwm stabilizes

        switch (control_state) {

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // temperature is low, fans idle
            case CONTROL_STATE_IDLE: {

                // temperature above threshold, start ramping up the fans
                if (max_temperature >= temp_sensor_int_to_q8_2(TEMP_REGULATION_START_TEMP)) control_state = CONTROL_STATE_IN_REGULATION;
                break;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // fans are ramping up or down
            case CONTROL_STATE_IN_REGULATION: {

                static uint16_t stable_pwm_count = 0;
                uint8_t prev_pwm = fan_get_pwm();

                int16_t pwm = TEMP_REGULATION_SLOPE * (max_temperature - temp_sensor_int_to_q8_2(TEMP_REGULATION_START_TEMP)) + FAN_MIN_PWM;

                if      (pwm < FAN_MIN_PWM) pwm = FAN_MIN_PWM;
                else if (pwm > FAN_MAX_PWM) pwm = FAN_MAX_PWM;

                fan_set_pwm(pwm);

                if (pwm == prev_pwm) {

                    // pwm was stable for 16 iterations, temperature is stable
                    if (++stable_pwm_count == 16) {

                        stable_temperature = max_temperature;
                        control_state = CONTROL_STATE_STABLE;
                    }

                }   else stable_pwm_count = 0;

                if (max_temperature >= temp_sensor_int_to_q8_2(TEMP_REGULATION_OTP_START_TEMP)) {
                    
                    load_trigger_fault(LOAD_FAULT_OTP);

                    control_state = CONTROL_STATE_IN_OTP;
                    fan_set_pwm(FAN_MAX_PWM);

                } else if (max_temperature <= temp_sensor_int_to_q8_2(TEMP_REGULATION_STOP_TEMP)) {
                    
                    fan_set_pwm(FAN_IDLE_PWM);
                    control_state = CONTROL_STATE_IDLE;     // transistors cooled down - go back to idle
                }

                break;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // temperature stabilized and fans are kept at constant speed
            case CONTROL_STATE_STABLE: {

                // temperature changed by 1 °C, go back to fan regulation
                     if (max_temperature >= stable_temperature + temp_sensor_int_to_q8_2(1)) control_state = CONTROL_STATE_IN_REGULATION;
                else if (max_temperature <= stable_temperature - temp_sensor_int_to_q8_2(1)) control_state = CONTROL_STATE_IN_REGULATION;

                break;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

            // overtemperature protection triggered
            case CONTROL_STATE_IN_OTP: {

                // load cooled down after an OTP trigger, stop the fans; the OTP fault remains triggered until clearing by master
                if (max_temperature <= temp_sensor_int_to_q8_2(TEMP_REGULATION_OTP_STOP_TEMP)) control_state = CONTROL_STATE_IN_REGULATION;
                break;
            }

            //- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        }

        //--------------------------------------------------------------------------------------------------------------------------------------------------------

        // write the measured temperature to its respective registers
        cmd_write(CMD_ADDRESS_TEMP_L, temp_sensor_q8_2_to_int(temperature[TEMP_L]));
        cmd_write(CMD_ADDRESS_TEMP_R, temp_sensor_q8_2_to_int(temperature[TEMP_R]));

        //--------------------------------------------------------------------------------------------------------------------------------------------------------

        kernel_sleep_ms(500);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
