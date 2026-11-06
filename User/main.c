/*
 * main.c
 *
 * Created on: 2025-08-01
 * Author: XF Team
 * Modified: 修复 L6218E 错误，补全 NVIC_Configuration 函数
 */

#include "main.h"
#include "mt_state_machine.h"
#include "mt_track_detect.h"
#include "mt_pid.h"
#include "mt_encoder.h"
#include "mt_vofa.h"
#include "mt_mpu6050.h"
#include "mt_ultrasonic.h"
#include "mt_motor.h"
#include "mt_mode_control.h"  
#include "stm32f10x.h"
#include "pwm_config.h"  
#include "LED.h" 
#include "Key.h" 
#include <stdio.h> 
#include <stdlib.h> 
#include "Delay.h" 

// 全局变量定义 
SystemState_e g_systemState = SYS_INIT;
volatile uint32_t g_systemTick = 0; 
volatile uint16_t g_adcBuffer[SENSOR_MAX];
/* 基础速度设定（PWM 占空比，满量程 1000）。当前为低速调试档。 */
#define BASE_SPEED_LEFT   90
#define BASE_SPEED_RIGHT  100
int setup_speed_l = BASE_SPEED_LEFT;
int setup_speed_r = BASE_SPEED_RIGHT; 

// 函数声明
void RCC_Configuration(void);
void GPIO_Configuration(void);
void NVIC_Configuration(void); // 声明在此
void TIM_Configuration(void);
void ADC_Configuration(void);
void USART_Configuration(void);
void I2C_Configuration(void);
uint32_t GetTick(void);
void Force_Fix_Conflict(void); 

// 蜂鸣器控制函数
void Buzzer_On(void) {
    GPIO_ResetBits(GPIOB, GPIO_Pin_15); 
}

void Buzzer_Off(void) {
    GPIO_SetBits(GPIOB, GPIO_Pin_15); 
}

void Buzzer_Beep(uint32_t duration_ms) {
    Buzzer_On();
    Delay_ms(duration_ms);
    Buzzer_Off();
}

void Buzzer_Tone(uint16_t frequency, uint32_t duration_ms) {
    if (frequency == 0) return;                  /* 防止除零 */
    uint32_t period = 1000000UL / frequency;      /* 周期，单位 us */
    if (period == 0) return; 
    uint32_t totalTicks = (duration_ms * 1000UL) / period; 
    
    for (uint32_t i = 0; i < totalTicks; i++) {
        Buzzer_On();
        Delay_us(period / 2);
        Buzzer_Off();
        Delay_us(period / 2);
    }
}

int main(void) {
    // 1. 系统底层初始化
    SystemInit();
    
    // 2.【必须最先初始化】SysTick定时器
    SysTick_Init();

    // 3. 基础外设时钟和GPIO初始化
    RCC_Configuration();
    GPIO_Configuration();
    
    Buzzer_Tone(500, 100);

    // 4. 其他外设配置
    NVIC_Configuration(); // 这里调用了，下面必须要有定义
    TIM_Configuration(); 
    
    // 5. ADC 初始化
    ADC_Configuration();
    USART_Configuration();

    // 6. 模块初始化
    LED_Init();
    Key_Init();
    /* 编码器未接入：TIM3 已让给其他外设，速度反馈改开环。
       恢复闭环时取消下一行注释，并同步启用主循环中的 Encoder_Update()。 */
    // Encoder_Init();
    Motor_Init();
    
    // 7. 传感器初始化
    if (MPU6050_Init() == 0) {
        while(1) {
            Buzzer_Tone(2000, 100);
            Delay_ms(100);
        }
    }
    
    Ultrasonic_Init();
    TrackDetect_Init();
    ModeControl_Init();
    MotorControl_Init();
    StateMachine_Init();
    Vofa_Init();
    
    /* 初始化结束后统一修正引脚复用冲突，避免各模块分别改写同一组 GPIO。 */
    Force_Fix_Conflict();
    
    // 初始化完成提示音
    Buzzer_Tone(1000, 200);

    /* PB6 / PB7 作为状态输入，需上拉以避免悬空误触发。 */
    GPIO_InitTypeDef GPIO_Repair;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_Repair.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_Repair.GPIO_Mode = GPIO_Mode_IPU; 
    GPIO_Repair.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_Repair);
    
    // 主循环控制变量
    uint32_t control_tick = 0;
    const uint32_t CONTROL_PERIOD = 2;     

    while (1) {
        /* 调试用：把 PB6 / PB7 电平镜像到板载 LED（PC13），便于现场查线。 */
        uint8_t p6 = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6);
        uint8_t p7 = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7);

        static uint8_t last_p6 = 0, last_p7 = 0;
        if (p6 != last_p6 || p7 != last_p7) {
            if (p6) GPIO_ResetBits(GPIOC, GPIO_Pin_13); 
            else    GPIO_SetBits(GPIOC, GPIO_Pin_13);
            last_p6 = p6;
            last_p7 = p7;
        }

        if (g_systemState == SYS_ERROR) {
            System_ErrorHandler(); 
        }
        
        KEY_STATE key1_state = Key_GetState(1);
        if (key1_state != KEY_NONE) {
            ModeControl_Key1_Handle(key1_state);
        }
        
        KEY_STATE key2_state = Key_GetState(2);
        if (key2_state != KEY_NONE) {
            ModeControl_Key2_Handle(key2_state);
        }
        
        ModeControl_Execute();
        
        static uint32_t ledTick = 0;
        if (GetTick() - ledTick > 500) { 
            ledTick = GetTick();
            LED1_Turn();
        }
        
        if (Delay_ms_NonBlock(control_tick, CONTROL_PERIOD)) {
            control_tick = Get_SysTick_ms(); 
            
            MPU6050_Update();

            // Encoder_Update();
            
            /* 编码器未接入，速度反馈固定为 0，控制实际为开环。
               恢复闭环时改为 Encoder_GetSpeed() 并启用上面的 Encoder_Update()。 */
            float spd_L = 0.0f; 
            float spd_R = 0.0f;
            g_motorController.currentSpeed = (spd_L + spd_R) / 2.0f;
            g_motorController.leftSpeedFeedback = spd_L;
            g_motorController.rightSpeedFeedback = spd_R;

            Ultrasonic_Update();
            Telemetry_Update();
            
            TrackDetect_Update();
            
                    // 【核心保护】丢线 50ms 停止
                    if (g_trackSensor.lostLineDuration >= 20) {
                // 1. 切换模式为停止
                ModeControl_SetMode(MODE_STOP);
                // 2. 强制将目标速度清零
                MotorControl_SetSpeed(0, 0);
                // 3. 蜂鸣器报警提示
                Buzzer_On();
            } else {
                Buzzer_Off();
            }
            
            if (g_mode_state.current_mode == MODE_TRACKING) {
                static uint8_t task_counter = 0;
                task_counter++;
                
                StateMachine_Update();
                MotorControl_Update();
                
                if (task_counter >= 2) task_counter = 0;
            }
            
            static uint8_t vofa_counter = 0;
            if ((++vofa_counter % 2) == 0) { 
                Vofa_SendData();
                vofa_counter = 0;
            }
        }
    }
}

void System_ErrorHandler(void) {
    while (1) {
        Buzzer_Tone(500, 500); 
        Delay_ms(500); 
    }
}

void RCC_Configuration(void) {
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | 
        RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO,
        ENABLE
    );
    
    // ADC 时钟 6 分频 (12MHz) - 高效且稳定
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    
    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4,
        ENABLE
    );
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
}

void GPIO_Configuration(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    
    // 1. 【核心修复】恢复这行代码！把 TIM2 彻底移走
    // 这会将 TIM2 的引脚映射到 PA15/PB3 等位置，彻底释放 PA0
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM2, ENABLE);
    
    // 2. 禁用 JTAG 释放 PB3/PB4 (保持不变)
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    
    // 3. 【注释掉】TIM3 的重映射 (既然不用编码器了，就别映射，让 PB0/PB1 回归纯真)
    // GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // KEY1 (PA5)
    GPIO_InitStructure.GPIO_Pin = KEY1_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(KEY1_PORT, &GPIO_InitStructure);
    
    // KEY2 (PB3)
    GPIO_InitStructure.GPIO_Pin = KEY2_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(KEY2_PORT, &GPIO_InitStructure);
    
    // Motor Dir (PB12/13)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // Encoder TIM4 (PB6/7)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // Encoder TIM3 (PB4/5)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // Ultrasonic Trig (PB0)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // Ultrasonic Echo (PB1)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // I2C2 (PB10/11) - SW I2C OD
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // USART1 TX (PA9)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // USART1 RX (PA10)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // ADC (PA0-PA4)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    // Buzzer (PB15)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_15);
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    // 【注释掉】EXTI 相关的配置 (既然用了轮询，就不需要中断线了)
    /*
    EXTI_InitTypeDef EXTI_InitStructure;
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource1);
    EXTI_InitStructure.EXTI_Line = EXTI_Line1;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling; 
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    */
}

// 【补全】缺失的 NVIC 配置函数
void NVIC_Configuration(void) {
    // NVIC_InitTypeDef NVIC_InitStructure;
    
    // 设置NVIC优先级分组为2位抢占优先级，2位响应优先级
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    
    // ------------------ 最高优先级中断（抢占优先级0）------------------
    // 1. 【注释掉】EXTI1 中断使能 (防止中断空转抢占 CPU)
    /*
    NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    */
    
    // 暂时禁用了 TIM2 和 USART1 中断以防止卡死
}

void TIM_Configuration(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    
    // 初始化 TIM3 作为超声波计时基准
    TIM_TimeBaseStructure.TIM_Period = 65535;
    TIM_TimeBaseStructure.TIM_Prescaler = 71;  // 72MHz / 72 = 1MHz (1us/tick)
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
    
    // 启动 TIM3
    TIM_Cmd(TIM3, ENABLE);
}

void ADC_Configuration(void) {
    ADC_InitTypeDef ADC_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, DISABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; 
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 5;
    ADC_Init(ADC1, &ADC_InitStructure);
    
    // ADC 采样时间优化：71.5 周期
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_71Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_71Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 3, ADC_SampleTime_71Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 4, ADC_SampleTime_71Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 5, ADC_SampleTime_71Cycles5);
    
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)g_adcBuffer;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = SENSOR_MAX;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    
    DMA_Cmd(DMA1_Channel1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);
    
    Delay_ms(5);
    
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
    
    Delay_ms(10);
    
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

void USART_Configuration(void) {
    USART_InitTypeDef USART_InitStructure;
    
    USART_InitStructure.USART_BaudRate = 9600;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);
    
    USART_Cmd(USART1, ENABLE);
}

void I2C_Configuration(void) {
    I2C_InitTypeDef I2C_InitStructure;
    
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1 = 0x00;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed = 100000;
    I2C_Init(I2C2, &I2C_InitStructure);
    
    I2C_Cmd(I2C2, ENABLE);
}

uint32_t GetTick(void) {
    return g_systemTick;
}

// ============================================================
// 强力修复冲突函数 (防止 PA1/PA2 被复用功能夺走)
// ============================================================
void Force_Fix_Conflict(void) {
    // 1. 关闭 USART2 (仍然保留，防止 PA2 干扰)
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, DISABLE);
    
    // 2. 强制配置 PA0-PA4 为模拟输入 (关键：恢复电感检测功能)
    uint32_t temp = GPIOA->CRL;
    temp &= 0xFFF00000;
    GPIOA->CRL = temp;
    
    // 3. TIM3 计数器已经在 TIM_Configuration 里启动了，这里不用管
}
