#include "temp_control.h"
#include "load_control.h"
#include "cmd_interface/cmd_spi_driver.h"

bool selfest_done = false;
uint16_t fault_flags = 0x0000;
uint32_t sensor_fault_cumulative_counter[2] = {0};
uint32_t fan_fault_cumulative_counter[2] = {0};

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// regulates fan speed based on power transistor temperature
void temp_control_task(void) {

    uint32_t fan_regulator_stack[64];
    kernel_create_task(fan_regulator_task, fan_regulator_stack, sizeof(fan_regulator_stack), 100);
    temp_sensor_init();

    while (1) {

        //---- TEMPERATURE MEASUREMENT ---------------------------------------------------------------------------------------------------------------------------
        
        uint8_t temp_l = temp_sensor_read(TEMP_L);
        uint8_t temp_r = temp_sensor_read(TEMP_R);

        cmd_write(CMD_ADDRESS_TEMP_L, temp_l);
        cmd_write(CMD_ADDRESS_TEMP_R, temp_r);

        //---- HANDLING SENSOR FAULTS ----------------------------------------------------------------------------------------------------------------------------

        for (int sensor = TEMP_L; sensor <= TEMP_R; sensor++) {

            temp_sensor_fault_t sensor_fault = temp_sensor_read_faults(sensor);

            // fault is short or open
            if (sensor_fault != TEMP_SEN_FAULT_NONE) {

                // trigger a sensor fault after after 4 cumulative faults
                if (++sensor_fault_cumulative_counter[sensor] == 4) {

                    write_masked(fault_flags, sensor_fault << (sensor * 2), 0x3 << (sensor * 2));
                    trigger_fault(LOAD_FAULT_TEMP_SENSOR);
                    cmd_write(CMD_ADDRESS_FAULT2, fault_flags);

                    sensor_fault_cumulative_counter[sensor] = 3;
                }

            // no fault, decrement the cumulative counter
            } else if (sensor_fault_cumulative_counter[sensor] > 0) sensor_fault_cumulative_counter[sensor]--;
        }

        //---- HANDLING FAN FAULTS -------------------------------------------------------------------------------------------------------------------------------

        uint8_t fan_pwm = fan_get_speed();
        
        // check the RPM of both fans
        for (int fan = FAN1; fan <= FAN2; fan++) {

            uint16_t fan_rpm = fan_get_rpm(fan);

            // fan PWM is above idle but no spin
            if (fan_pwm > 30 && fan_rpm == 0) {

                if (++fan_fault_cumulative_counter[fan] == 8) {

                    set_bits(fault_flags, 1 << (4 + fan));
                    trigger_fault(LOAD_FAULT_FAN);
                    cmd_write(CMD_ADDRESS_FAULT2, fault_flags);

                    fan_fault_cumulative_counter[fan] = 7; 
                }

            } else if (fan_fault_cumulative_counter > 0) fan_fault_cumulative_counter[fan]--;

            cmd_write((fan == FAN1) ? CMD_ADDRESS_RPM1 : CMD_ADDRESS_RPM2, fan_pwm);
        }

        //---- FAN CONTROL ---------------------------------------------------------------------------------------------------------------------------------------

        uint8_t max_temp = (temp_l > temp_r) ? temp_l : temp_r;

        if (max_temp > 70 && max_temp < 255) {

            if (max_temp == temp_l) set_bits(fault_flags, 1 << (6 + 0));
            if (max_temp == temp_r) set_bits(fault_flags, 1 << (6 + 1));
            trigger_fault(LOAD_FAULT_OTP);
            cmd_write(CMD_ADDRESS_FAULT2, fault_flags);

        } else if (max_temp > 50) {

            int16_t pwm = 11 * (max_temp - 50) + 30;
            if (pwm > 255) pwm = 255;

            fan_set_speed(pwm);

        } else fan_set_speed(FAN_IDLE_SPEED);

        //--------------------------------------------------------------------------------------------------------------------------------------------------------

        kernel_sleep_ms(500);
    }
}

void temp_control_clear_faults(void) {

    fault_flags = 0x00;
    sensor_fault_cumulative_counter[0] = 0;
    sensor_fault_cumulative_counter[1] = 0;
    fan_fault_cumulative_counter[0] = 0;
    fan_fault_cumulative_counter[1] = 0;
    cmd_write(CMD_ADDRESS_FAULT2, fault_flags);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
