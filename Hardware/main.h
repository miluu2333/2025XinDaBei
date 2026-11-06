/*
 * main.h
 *
 * Created on: 2025-08-01
 * Author: XF Team
 * Modified: 修正引脚定义以匹配实际硬件
 */

#ifndef MAIN_H_
#define MAIN_H_

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>

// STM32标准库头文件
#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_adc.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_i2c.h"
#include "misc.h"

// 系统配置参数
#define SYSTEM_FREQ_HZ       72000000
#define CONTROL_FREQ_HZ      200         // 控制频率 200Hz
#define CONTROL_PERIOD_MS    5           // 控制周期 5ms

// 引脚定义

// 1. PWM输出 (DRV8701E) - 图片: PA8/PA11
#define PWM_LEFT_PIN         GPIO_Pin_8
#define PWM_RIGHT_PIN        GPIO_Pin_11
#define PWM_PORT             GPIOA
#define PWM_TIM              TIM1
#define PWM_FREQ_HZ          10000       // 10kHz PWM

// 2. 电机方向 - 图片: PB12/PB13
#define DIR_LEFT_PIN         GPIO_Pin_12
#define DIR_RIGHT_PIN        GPIO_Pin_13
#define DIR_PORT             GPIOB

// 3. 编码器
// 编码器1 (TIM4) - 图片: PB6/PB7
#define ENCODER_LEFT_CH1     GPIO_Pin_6
#define ENCODER_LEFT_CH2     GPIO_Pin_7
#define ENCODER_LEFT_PORT    GPIOB
#define ENCODER_LEFT_TIM     TIM4

// 编码器2 (TIM3) - 图片: PB4/PB5 (需要重映射!)
#define ENCODER_RIGHT_CH1    GPIO_Pin_4
#define ENCODER_RIGHT_CH2    GPIO_Pin_5
#define ENCODER_RIGHT_PORT   GPIOB
#define ENCODER_RIGHT_TIM    TIM3
#define ENCODER_PORT         GPIOB // 通用定义

// 4. 串口通信 (蓝牙模块 USART1) - 图片: PA9/PA10
#define UART_TX_PIN          GPIO_Pin_9
#define UART_RX_PIN          GPIO_Pin_10
#define UART_PORT            GPIOA
#define UART                 USART1

// 5. 电感传感器ADC通道 (PA0-PA4) - 图片: PA0-PA4
#define ADC_CHANNEL_L2       ADC_Channel_0    // PA0
#define ADC_CHANNEL_L1       ADC_Channel_1    // PA1
#define ADC_CHANNEL_M        ADC_Channel_2    // PA2
#define ADC_CHANNEL_R1       ADC_Channel_3    // PA3
#define ADC_CHANNEL_R2       ADC_Channel_4    // PA4

// 6. 超声波引脚 (PB0/PB1) - 图片: PB0/PB1
#define ULTRASONIC_TRIG_PIN  GPIO_Pin_0
#define ULTRASONIC_ECHO_PIN  GPIO_Pin_1
#define ULTRASONIC_PORT      GPIOB

// 7. I2C接口 (MPU6050/OLED) - 图片: PB10/PB11 (I2C2)
// 注意：这是 I2C2 的引脚
#define I2C_SCL_PIN          GPIO_Pin_10
#define I2C_SDA_PIN          GPIO_Pin_11
#define I2C_PORT             GPIOB
#define I2C_PERIPH           I2C2

// 8. 蜂鸣器 - 图片: PB15
#define BUZZER_PIN           GPIO_Pin_15
#define BUZZER_PORT          GPIOB

// 9. 按键 - 图片: PA5/PB3
#define KEY1_PIN             GPIO_Pin_5
#define KEY1_PORT            GPIOA
#define KEY2_PIN             GPIO_Pin_3
#define KEY2_PORT            GPIOB

// 10. 干簧管 - 图片: PC13
#define REED_SWITCH_PIN      GPIO_Pin_13
#define REED_SWITCH_PORT     GPIOC

// 系统状态定义
typedef enum {
    SYS_INIT = 0,
    SYS_READY,
    SYS_RUNNING,
    SYS_ERROR
} SystemState_e;

// 全局变量声明
extern SystemState_e g_systemState;
extern volatile uint32_t g_systemTick; // 加volatile确保中断更新

// 函数声明
void System_Init(void);
void System_Start(void);
void System_ErrorHandler(void);

// 系统时钟和延时函数
uint32_t GetTick(void);
// 注意：delay_ms和delay_us函数已移至Delay.h中，使用Delay_ms和Delay_us代替

#endif /* MAIN_H_ */
