#include "fan_control.h"

//---- PRIVATE DATA ----------------------------------------------------------------------------------------------------------------------------------------------

typedef struct {

    volatile uint32_t tach_pulse_count;
    volatile kernel_time_t last_tach_pulse_time;

} fan_tach_t;

static uint8_t set_speed = 0;                   // target fan speed set by the temperature control logic
static uint8_t set_speed_override = 0;          // target speed set by the control logic or speed override set externally is selected based on which is greater (override cannot decrease the speed)
static uint8_t target_speed = 0;                // real target speed; equal to set_speed if set_speed is greater then set_speed_override; equal to set_speed_override otherwise
static uint8_t current_speed = 0;               // real fan speed, slowly ramped to reach the target
static fan_tach_t fan[2];                       // data used by the RPM measurement logic

//---- PRIVATE FUNCTIONS -----------------------------------------------------------------------------------------------------------------------------------------

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

        fan[i].tach_pulse_count = 60 * FAN_TACH_REF_CLOCK_HZ / FAN_TACH_PULSES_PER_ROTATION;
        fan[i].last_tach_pulse_time = 0;
    }

    // initialize fan control PWM
    timer_init_pwm(FAN1_PWM_TIMER, FAN1_PWM_GPIO, FAN_PWM_FREQUENCY_HZ, FAN_PWM_RELOAD_VAL);
    timer_init_pwm(FAN2_PWM_TIMER, FAN2_PWM_GPIO, FAN_PWM_FREQUENCY_HZ, FAN_PWM_RELOAD_VAL);

    // initialize frequency counter and gpio interrupt for fan speed measurement
    timer_init_counter(FAN1_TACH_TIMER, FAN_TACH_REF_CLOCK_HZ, TIMER_DIR_UP, 0xffff);
    timer_init_counter(FAN2_TACH_TIMER, FAN_TACH_REF_CLOCK_HZ, TIMER_DIR_UP, 0xffff);
    gpio_init_interrupt(FAN1_TACH_GPIO, GPIO_IRQ_FALLING_EDGE);
    gpio_init_interrupt(FAN2_TACH_GPIO, GPIO_IRQ_FALLING_EDGE);

    while (1) {

        while (current_speed == target_speed) kernel_yield();       // nothing to do, yield with low priority

        if (target_speed > current_speed) {

            current_speed++;        // slowly ramp-up the speed to minimize current draw
            timer_set_pwm_duty(FAN1_PWM_TIMER, current_speed);
            timer_set_pwm_duty(FAN2_PWM_TIMER, current_speed);

        } else if (target_speed < current_speed) {

            current_speed = target_speed;
            timer_set_pwm_duty(FAN1_PWM_TIMER, current_speed);
            timer_set_pwm_duty(FAN2_PWM_TIMER, current_speed);
        }
        
        kernel_sleep_ms(FAN_RAMP_UP_SLOPE / FAN_PWM_RELOAD_VAL);
    }
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the target fan speed for both fans
void fan_set_speed(uint8_t speed) {

    set_speed = speed;

    // target speed set by the control logic or speed override set externally is selected based on which is greater (override cannot decrease the speed)
    target_speed = (set_speed_override > set_speed) ? set_speed_override : set_speed;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the current fan speed for both fans
uint8_t fan_get_speed(void) {

    return (current_speed);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets the target fan speed override for both fans
// this is used to control the fan speed externally for example by a user; the override cannot set a lower speed than the speed set by the temperature control logic
void fan_set_speed_override(uint8_t speed) {

    set_speed_override = speed;

    // target speed set by the control logic or speed override set externally is selected based on which is greater (override cannot decrease the speed)
    target_speed = (set_speed_override > set_speed) ? set_speed_override : set_speed;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// returns the speed of FAN1 or FAN2 in RPM
uint16_t fan_get_rpm(fan_t fan_num) {

    if (fan_num != FAN1 && fan_num != FAN2) return 0;

    uint32_t pulse_count = fan[fan_num].tach_pulse_count;
    if ((pulse_count == 0) || (kernel_get_time_since(fan[fan_num].last_tach_pulse_time) > FAN_TACH_NO_ROTATION_DETECT_TIME)) return 0;  // if the last pulse was detected a long time ago, the fan is not spinning anymore

    return (60 * FAN_TACH_REF_CLOCK_HZ / FAN_TACH_PULSES_PER_ROTATION / pulse_count);
}

//---- IRQ HANDLERS ----------------------------------------------------------------------------------------------------------------------------------------------

static force_inline void tach_interrupt_handler(uint8_t fan_num) {

    if (EXTI->PR & get_fan_tach_exti_line(fan_num)) {

        timer_stop_count(get_fan_tach_timer(fan_num));

        fan[fan_num].tach_pulse_count = timer_get_count(get_fan_tach_timer(fan_num));
        fan[fan_num].last_tach_pulse_time = kernel_get_time_ms();

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
