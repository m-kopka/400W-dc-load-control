#ifndef _CONFIG_H_
#define _CONFIG_H_

//---- TEMPERATURE CONTROL ---------------------------------------------------------------------------------------------------------------------------------------

#define FAN_RAMP_UP_SLOPE   5000    // fan ramp up time from 0 to max speed [ms]. Limited slope minimizes current draw
#define FAN_IDLE_SPEED      0       // fan speed when the load is idle

#define FAN_TEST_SPEED      128     // fan speed during the power-up test
#define FAN_TEST_DURATION   3000    // time duration for letting the fans spin-up during the power-up test [ms]
#define FAN_TEST_FAIL_RPM   10000   // if the measured fan rpm during the power-up test is lower than this, the test will fail [rpm]

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _CONFIG_H_ */