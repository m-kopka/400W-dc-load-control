#ifndef _HW_CONFIG_H_
#define _HW_CONFIG_H_

//---- SYSTEM ----------------------------------------------------------------------------------------------------------------------------------------------------

#define HSE_OSC_FREQUENCY_HZ    25000000        // High Speed External oscillator frequency [Hz]
#define CORE_CLOCK_FREQUENCY_HZ 96000000        // core clock frequency [Hz]

//---- DEBUG UART ------------------------------------------------------------------------------------------------------------------------------------------------

#define DEBUG_UART              USART1
#define DEBUG_UART_BAUD         115200
#define DEBUG_UART_TX_GPIO      GPIOB, 6
#define DEBUG_UART_RX_GPIO      GPIOB, 7

//---- LEDS ------------------------------------------------------------------------------------------------------------------------------------------------------

// green LED
#define LOAD_ENABLE_LED_GPIO_CLOCK      RCC_PERIPH_AHB1_GPIOC
#define LOAD_ENABLE_LED_GPIO            GPIOC, 14

// red LED
#define FAULT_LED_GPIO_CLOCK        RCC_PERIPH_AHB1_GPIOC
#define FAULT_LED_GPIO              GPIOC, 13

//---- CMD SPI ---------------------------------------------------------------------------------------------------------------------------------------------------

#define CMD_SPI                 SPI3
#define CMD_SPI_CLOCK           RCC_PERIPH_APB1_SPI3
#define CMD_SPI_GPIO_AF         GPIO_ALTERNATE_FUNCTION_SPI2_SPI3_SPI4_SPI5
#define CMD_SPI_IRQ             SPI3_IRQn
#define CMD_SPI_IRQ_HANDLER     SPI3_Handler

#define CMD_SPI_SS_GPIO_CLOCK   RCC_PERIPH_AHB1_GPIOA
#define CMD_SPI_SS_GPIO         GPIOA, 15
#define CMD_SPI_SCK_GPIO_CLOCK  RCC_PERIPH_AHB1_GPIOC
#define CMD_SPI_SCK_GPIO        GPIOC, 10
#define CMD_SPI_MOSI_GPIO_CLOCK RCC_PERIPH_AHB1_GPIOC
#define CMD_SPI_MOSI_GPIO       GPIOC, 12
#define CMD_SPI_MISO_GPIO_CLOCK RCC_PERIPH_AHB1_GPIOC
#define CMD_SPI_MISO_GPIO       GPIOC, 11

//---- ISET DAC --------------------------------------------------------------------------------------------------------------------------------------------------

#define ISET_DAC_SPI            SPI1
#define ISET_DAC_SPI_CLOCK      RCC_PERIPH_APB2_SPI1
#define ISET_DAC_SPI_DIV        SPI_DIV_4                                   // PCLK2 = 96MHz, 96MHz / 4 = 24MHz
#define ISET_DAC_GPIO_AF        GPIO_ALTERNATE_FUNCTION_SPI1_SPI2_SPI3

#define ISET_DAC_SPI_SS_GPIO_CLOCK      RCC_PERIPH_AHB1_GPIOA
#define ISET_DAC_SPI_SS_GPIO            GPIOA, 4
#define ISET_DAC_SPI_SCK_GPIO_CLOCK     RCC_PERIPH_AHB1_GPIOA
#define ISET_DAC_SPI_SCK_GPIO           GPIOA, 5
#define ISET_DAC_SPI_MOSI_GPIO_CLOCK    RCC_PERIPH_AHB1_GPIOA
#define ISET_DAC_SPI_MOSI_GPIO          GPIOA, 7

#define ISET_DAC_TIMER                  TIM9
#define ISET_DAC_TIMER_IRQ              TIM1_BRK_TIM9_IRQn
#define ISET_DAC_TIMER_IRQ_HANDLER      TIM1_BRK_TIM9_Handler
#define ISET_DAC_TIMER_FREQUENCY        1000

#define ISET_DAC_LSB_PER_MA(i_ma)       (-14836 * (int32_t)(i_ma) / 10000)
#define ISET_DAC_MA_TO_CODE(i_ma)    (ISET_DAC_LSB_PER_MA(i_ma) + 62569)

//---- VSEN ADC --------------------------------------------------------------------------------------------------------------------------------------------------

#define VSEN_ADC_SPI            SPI5
#define VSEN_ADC_SPI_CLOCK      RCC_PERIPH_APB2_SPI5
#define VSEN_ADC_SPI_DIV        SPI_DIV_4                                   // PCLK2 = 96MHz, 96MHz / 4 = 24MHz
#define VSEN_ADC_GPIO_AF        GPIO_ALTERNATE_FUNCTION_SPI2_SPI3_SPI4_SPI5

#define VSEN_ADC_SPI_SS_GPIO_CLOCK      RCC_PERIPH_AHB1_GPIOB
#define VSEN_ADC_SPI_SS_GPIO            GPIOB, 1
#define VSEN_ADC_SPI_SCK_GPIO_CLOCK     RCC_PERIPH_AHB1_GPIOB
#define VSEN_ADC_SPI_SCK_GPIO           GPIOB, 0
#define VSEN_ADC_SPI_MISO_GPIO_CLOCK    RCC_PERIPH_AHB1_GPIOA
#define VSEN_ADC_SPI_MISO_GPIO          GPIOA, 12

#define VSEN_ADC_CONVST_GPIO_CLOCK      RCC_PERIPH_AHB1_GPIOC
#define VSEN_ADC_CONVST_GPIO            GPIOC, 5
#define VSEN_SRC_GPIO_CLOCK             RCC_PERIPH_AHB1_GPIOB
#define VSEN_SRC_GPIO                   GPIOB, 2

#define VSEN_ADC_RAW_TO_MV_INT(raw)     (((raw) * 516 / 25) - 91)
#define VSEN_ADC_RAW_TO_MV_REM(raw)     (((raw) * 2069 / 100) - 91)

//---- ISEN ADC --------------------------------------------------------------------------------------------------------------------------------------------------

#define ISEN_ADC_SPI            SPI2
#define ISEN_ADC_SPI_CLOCK      RCC_PERIPH_APB1_SPI2
#define ISEN_ADC_SPI_DIV        SPI_DIV_2                                   // PCLK1 = 48MHz, 48MHz / 2 = 24MHz
#define ISEN_ADC_GPIO_AF        GPIO_ALTERNATE_FUNCTION_SPI1_SPI2_SPI3

#define ISEN_ADC_SPI_SS_GPIO_CLOCK      RCC_PERIPH_AHB1_GPIOB
#define ISEN_ADC_SPI_SS_GPIO            GPIOB, 12
#define ISEN_ADC_SPI_SCK_GPIO_CLOCK     RCC_PERIPH_AHB1_GPIOB
#define ISEN_ADC_SPI_SCK_GPIO           GPIOB, 13
#define ISEN_ADC_SPI_MISO_GPIO_CLOCK    RCC_PERIPH_AHB1_GPIOB
#define ISEN_ADC_SPI_MISO_GPIO          GPIOB, 14

#define ISEN_ADC_CONVST_GPIO_CLOCK      RCC_PERIPH_AHB1_GPIOB
#define ISEN_ADC_CONVST_GPIO            GPIOB, 15

#define ISEN_ADC_RAW_TO_MA(raw)     (((raw) * 1075 / 100) - 50)

//---- POWER BOARD ENABLE GPIOS ----------------------------------------------------------------------------------------------------------------------------------

#define LOAD_EN_L_GPIO_CLOCK    RCC_PERIPH_AHB1_GPIOA
#define LOAD_EN_L_GPIO          GPIOA, 9

#define LOAD_EN_R_GPIO_CLOCK    RCC_PERIPH_AHB1_GPIOC
#define LOAD_EN_R_GPIO          GPIOC, 2

//---- INTERNAL ADC CURRENT SENSE --------------------------------------------------------------------------------------------------------------------------------

#define I_SEN_L1_GPIO_CLOCK     RCC_PERIPH_AHB1_GPIOA
#define I_SEN_L1_GPIO           GPIOA, 3
#define I_SEN_L1_ADC_CH         3

#define I_SEN_L2_GPIO_CLOCK     RCC_PERIPH_AHB1_GPIOA
#define I_SEN_L2_GPIO           GPIOA, 0
#define I_SEN_L2_ADC_CH         0

#define I_SEN_R1_GPIO_CLOCK     RCC_PERIPH_AHB1_GPIOA
#define I_SEN_R1_GPIO           GPIOA, 1
#define I_SEN_R1_ADC_CH         1

#define I_SEN_R2_GPIO_CLOCK     RCC_PERIPH_AHB1_GPIOA
#define I_SEN_R2_GPIO           GPIOA, 2
#define I_SEN_R2_ADC_CH         2

//---- POWER TRANSISTOR TEMPERATURE SENSORS ----------------------------------------------------------------------------------------------------------------------

#define TEMP_SEN_L_GPIO_CLOCK   RCC_PERIPH_AHB1_GPIOC
#define TEMP_SEN_L_GPIO         GPIOC, 1
#define TEMP_SEN_L_ADC_CH       11

#define TEMP_SEN_R_GPIO_CLOCK   RCC_PERIPH_AHB1_GPIOC
#define TEMP_SEN_R_GPIO         GPIOC, 0
#define TEMP_SEN_R_ADC_CH       10

#define TEMP_SEN_READ_ATTEMPTS      3
#define TEMP_SEN_OPEN_THRESHOLD     30
#define TEMP_SEN_SHORT_THRESHOLD    30

//---- FANS ------------------------------------------------------------------------------------------------------------------------------------------------------

#define FAN1_PWM_GPIO_CLOCK     RCC_PERIPH_AHB1_GPIOB
#define FAN1_PWM_GPIO           GPIOB, 8
#define FAN1_PWM_TIMER_CH       TIM4, 3
#define FAN1_TACH_GPIO_CLOCK    RCC_PERIPH_AHB1_GPIOB
#define FAN1_TACH_GPIO          GPIOB, 5
#define FAN1_TACH_TIMER         TIM3
#define FAN1_TACH_EXTI_LINE     EXTI_PR_PR5
#define FAN1_TACH_IRQ_HANDLER   EXTI9_5_Handler

#define FAN2_PWM_GPIO_CLOCK     RCC_PERIPH_AHB1_GPIOB
#define FAN2_PWM_GPIO           GPIOB, 3
#define FAN2_PWM_TIMER_CH       TIM2, 2
#define FAN2_TACH_GPIO_CLOCK    RCC_PERIPH_AHB1_GPIOA
#define FAN2_TACH_GPIO          GPIOA, 11
#define FAN2_TACH_TIMER         TIM5
#define FAN2_TACH_EXTI_LINE     EXTI_PR_PR11
#define FAN2_TACH_IRQ_HANDLER   EXTI15_10_Handler

#define FAN_PWM_FREQUENCY_HZ    25000       // PWM frequency of both fans [Hz]
#define FAN_PWM_RELOAD_VAL      255         // PWM resolution of both fans

#define FAN_TACH_REF_CLOCK_HZ               100000      // reference clock frequency for fan RPM measurement [Hz]. Hardware counts pulses of this clock per fan half-rotation
#define FAN_TACH_PULSES_PER_ROTATION        2           // number of FAN_TACH pin pulses per rotation
#define FAN_TACH_NO_ROTATION_DETECT_TIME    150         // if the last FAN_TACH pulse is older than this time [ms]. The measurement logic will evaluate this as no fan rotation

//---- EXTERNAL FAULT PIN ----------------------------------------------------------------------------------------------------------------------------------------

#define EXT_FAULT_GPIO_CLOCK    RCC_PERIPH_AHB1_GPIOA
#define EXT_FAULT_GPIO          GPIOA, 8

//----------------------------------------------------------------------------------------------------------------------------------------------------------------

#endif /* _HW_CONFIG_H_ */