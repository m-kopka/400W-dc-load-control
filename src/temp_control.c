#include "temp_control.h"

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// regulates fan speed based on power transistor temperature
void temp_control_task(void) {

    kernel_create_task(fan_regulator_task, 100);
    temp_sensor_init();

    while (1) {

        uint8_t temp_l = temp_sensor_read(TEMP_L);
        uint8_t temp_r = temp_sensor_read(TEMP_R);
        uint8_t max_temp = (temp_l > temp_r) ? temp_l : temp_r;

        if (max_temp > 70) {

            gpio_write(LOAD_EN_L_GPIO, LOW);
            gpio_write(LOAD_EN_R_GPIO, LOW);

            debug_print("OTP triggered!\n");

        } else if (max_temp > 40) {

            int16_t pwm = 7 * (max_temp - 40) + 30;
            if (pwm > 255) pwm = 255;

            fan_set_speed(pwm);

        } else fan_set_speed(FAN_IDLE_SPEED);

        kernel_sleep_ms(1000);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
