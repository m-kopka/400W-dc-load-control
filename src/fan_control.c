#include "fan_control.h"
#include "hal/timer.h"

//---- INTERNAL DATA ---------------------------------------------------------------------------------------------------------------------------------------------

static uint8_t set_pwm = 0;                     // target fan pwm set by the temperature control logic
static uint8_t set_pwm_override = 0;            // target pwm set by the control logic or pwm override set externally is selected based on which is greater (override cannot decrease the speed)
static uint8_t target_pwm = 0;                  // real target pwm; equal to set_pwm if set_pwm is greater then set_pwm_override; equal to set_pwm_override otherwise
volatile uint32_t tach_pulse_count[2];          // updated by the frequency counter interrupt; holds number of reference clock pulses per fan half-rotation
volatile kernel_time_t last_tach_pulse_time[2]; // time of previous tach pulse [ms]; used for no spin detection

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

static inline uint32_t get_fan_tach_exti_line(uint8_t fan_num) {

    return (fan_num == 0) ? FAN1_TACH_EXTI_LINE : FAN2_TACH_EXTI_LINE;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

static inline TIM_TypeDef* get_fan_tach_timer(uint8_t fan_num) {

    return (fan_num == 0) ? FAN1_TACH_TIMER : FAN2_TACH_TIMER;
}

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes hardware for fan PWM control and speed measurement. Then continuously controls the fan speed based on the set point
void fan_regulator_task(void) {

    // initialize the fan tach structure for each fan
    for (int i = 0; i < 2; i++) {

        tach_pulse_count[i] = 60 * FAN_TACH_REF_CLOCK_HZ / FAN_TACH_PULSES_PER_ROTATION;
        last_tach_pulse_time[i] = 0;
    }

    // initialize fan control PWM
    timer_init_pwm(FAN1_PWM_TIMER_CH, FAN1_PWM_GPIO, FAN_PWM_FREQUENCY_HZ, FAN_PWM_RELOAD_VAL);
    timer_init_pwm(FAN2_PWM_TIMER_CH, FAN2_PWM_GPIO, FAN_PWM_FREQUENCY_HZ, FAN_PWM_RELOAD_VAL);

    // initialize frequency counter and gpio interrupt for fan speed measurement
    timer_init_counter(FAN1_TACH_TIMER, FAN_TACH_REF_CLOCK_HZ, TIMER_DIR_UP, 0xffff);
    timer_init_counter(FAN2_TACH_TIMER, FAN_TACH_REF_CLOCK_HZ, TIMER_DIR_UP, 0xffff);
    gpio_init_interrupt(FAN1_TACH_GPIO, GPIO_IRQ_FALLING_EDGE);
    gpio_init_interrupt(FAN2_TACH_GPIO, GPIO_IRQ_FALLING_EDGE);

    while (1) {

        while (timer_get_pwm_duty(FAN1_PWM_TIMER_CH) == target_pwm) kernel_yield();       // nothing to do, yield with low priority

        uint8_t pwm = timer_get_pwm_duty(FAN1_PWM_TIMER_CH);

        if (target_pwm > pwm) pwm++;
        else pwm--;
        
        timer_set_pwm_duty(FAN1_PWM_TIMER_CH, pwm);
        timer_set_pwm_duty(FAN2_PWM_TIMER_CH, pwm);
        
        kernel_sleep_ms(FAN_RAMP_SLOPE / FAN_PWM_RELOAD_VAL);
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the target fan pwm for both fans
void fan_set_pwm(uint8_t pwm) {

    set_pwm = pwm;

    // target speed set by the control logic or speed override set externally is selected based on which is greater (override cannot decrease the speed)
    target_pwm = (set_pwm_override > set_pwm) ? set_pwm_override : set_pwm;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the current fan pwm for both fans
uint8_t fan_get_pwm(void) {

    return (timer_get_pwm_duty(FAN1_PWM_TIMER_CH));
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the target fan pwm override for both fans
// this is used to control the fan speed externally for example by a user; the override cannot set a lower pwm than the value set by the temperature control logic
void fan_set_pwm_override(uint8_t speed) {

    set_pwm_override = speed;

    // target speed set by the control logic or speed override set externally is selected based on which is greater (override cannot decrease the speed)
    target_pwm = (set_pwm_override > set_pwm) ? set_pwm_override : set_pwm;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the speed of FAN1 or FAN2 in RPM
uint16_t fan_get_rpm(fan_t fan_num) {

    if (fan_num != FAN1 && fan_num != FAN2) return 0;

    uint32_t pulse_count = tach_pulse_count[fan_num];

    // if the last pulse was detected a long time ago, the fan is not spinning anymore
    if ((pulse_count == 0) || (kernel_get_time_since(last_tach_pulse_time[fan_num]) > FAN_TACH_NO_ROTATION_DETECT_TIME)) return 0; 

    return (60 * FAN_TACH_REF_CLOCK_HZ / FAN_TACH_PULSES_PER_ROTATION / pulse_count);
}

//---- IRQ HANDLERS ----------------------------------------------------------------------------------------------------------------------------------------------

static force_inline void tach_interrupt_handler(uint8_t fan_num) {

    if (EXTI->PR & get_fan_tach_exti_line(fan_num)) {

        timer_stop_count(get_fan_tach_timer(fan_num));

        tach_pulse_count[fan_num] = timer_get_count(get_fan_tach_timer(fan_num));
        last_tach_pulse_time[fan_num] = kernel_get_time_ms();

        timer_reset_count(get_fan_tach_timer(fan_num));
        timer_start_count(get_fan_tach_timer(fan_num));

        EXTI->PR |= get_fan_tach_exti_line(fan_num);    // clear the EXTI pending flag
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// triggered on a falling edge of FAN1_TACH
void FAN1_TACH_IRQ_HANDLER(void) {

    tach_interrupt_handler(FAN1);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// triggered on a falling edge of FAN2_TACH
void FAN2_TACH_IRQ_HANDLER(void) {

    tach_interrupt_handler(FAN2);
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
