#ifndef _CONFIG_H_
#define _CONFIG_H_

//---- TEMPERATURE CONTROL ---------------------------------------------------------------------------------------------------------------------------------------

#define FAN_RAMP_SLOPE      5000    // fan ramp up time from 0 to max speed [ms]. Limited slope minimizes current draw
#define FAN_IDLE_PWM        0       // fan pwm when the load is idle
#define FAN_MIN_PWM         30      // minimum fan pwm to start the fan
#define FAN_MAX_PWM         217     // maximum fan pwm

#define FAN_TEST_ENABLED    1       // on startup, the temp_control_task ramps up the fans and checks if rotation is detected
#define FAN_TEST_SPEED      128     // fan speed during the power-up test
#define FAN_TEST_DURATION   3000    // time duration for letting the fans spin-up during the power-up test [ms]
#define FAN_TEST_FAIL_RPM   10000   // if the measured fan rpm during the power-up test is lower than this, the test will fail [rpm]

#define TEMP_REGULATION_START_TEMP      50      // temperature threshold for starting the temperature regulation [°C]
#define TEMP_REGULATION_STOP_TEMP       48      // temperature threshold for stopping the temperature regulation [°C]
#define TEMP_REGULATION_OTP_START_TEMP  70      // temperature threshold for triggering the overtemperature protection [°C]
#define TEMP_REGULATION_OTP_STOP_TEMP   60      // temperature threshold for going back to fan regulation from OTP [°C] (OTP fault remains triggered until cleared)

// slope of temperature regulation; in pwm value per 0,25 degrees of temperature change
// PWM will be at FAN_MIN_PWM at the temperature of TEMP_REGULATION_START_TEMP and wil reach FAN_MAX_PWM before the temperature of TEMP_REGULATION_OTP_START_TEMP
#define TEMP_REGULATION_SLOPE       (((((FAN_MAX_PWM) - (FAN_MIN_PWM)) / ((TEMP_REGULATION_OTP_START_TEMP) - (TEMP_REGULATION_START_TEMP))) + 3) / 4)

#define TEMP_SENSOR_FAULT_CUMULATIVE_THRESHOLD      4   // temperature sensor fault is triggered after n cumulative faults
#define FAN_FAULT_CUMULATIVE_THRESHOLD              8   // fan fault is triggered after n cumulative faults

//---- LOAD CONTROL ----------------------------------------------------------------------------------------------------------------------------------------------

#define LOAD_MIN_CURRENT_MA     300     // minimum CC level allowed [mA]
#define LOAD_MAX_CURRENT_MA     42000   // maximum CC level allowed [mA]

#define LOAD_AVAILABLE_CURRENT_A    (LOAD_MAX_CURRENT_MA / 1000)    // available current advertized to the master [A]
#define LOAD_AVAILABLE_POWER_W      420                             // available power advertized to the master [W]

#define LOAD_OCP_THRESHOLD_MA       45000   // if the load current is higher than this value, OCP fault is triggered
#define LOAD_OPP_THRESHOLD_MW       430000  // if the load power is higher than this value, OPP fault is triggered
#define LOAD_NO_REG_THRESHOLD_CC    200     // if the current difference in CC mode is higher than this value NO_REG flag will be raised
#define LOAD_NO_REG_THRESHOLD_CV    1000    // if the voltage difference in CV mode is higher than this value NO_REG flag will be raised
#define LOAD_NO_REG_THRESHOLD_CR    10000   // if the resistance difference in CR mode is higher than this value NO_REG flag will be raised
#define LOAD_NO_REG_THRESHOLD_CP    10000   // if the power difference in CP mode is higher than this value NO_REG flag will be raised

#define LOAD_NO_REG_CUMULATIVE_COUNTS       16   // NO_REG flag will be raised after n cumulative no reg events
#define LOAD_FUSE_FAULT_CUMULATIVE_COUNTS   16  // FUSE fault will be triggered after n cumulative faults

#define LOAD_CONTROL_UPDATE_PERIOD_MS   100     // time period for checking the OPP and OCP state and updating load statistics [ms]
#define LOAD_START_CC_LEVEL_MA          1000    // CC level on startup [mA]

//---- ISET DAC --------------------------------------------------------------------------------------------------------------------------------------------------

#define SLEW_LIMIT_AMPS_PER_SECOND  20      // load slew rate when changing CC level or enabling the load [A/S]

//---- VOLTAGE AND CURRENT SENSE ---------------------------------------------------------------------------------------------------------------------------------

#define VI_SENSE_MIN_VOLTAGE 100        // lowest measured voltage (samples less than this will be equal to 0 mV)
#define VI_SENSE_MIN_CURRENT 300        // lowest measured current (samples less than this will be equal to 0 mA)

#define INTERNAL_ISEN_MIN_CURRENT 50     // lowest measured current of a single current sink (samples less than this will be equal to 0 mA)

#define VI_SENSE_AUTO_VSENSRC_THRESHOLD_MV    100   // threshold meassured voltage for automatic switching of VSEN source [mV]

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _CONFIG_H_ */