#include "stm32f10x.h"
#include "main.h"
#include "pwm_config.h"

// PWM初始化（TIM1，17kHz）
void PWM_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 | RCC_APB2Periph_GPIOA, ENABLE);
    
    // 配置PWM引脚（PA8, PA11）
    GPIO_InitStructure.GPIO_Pin = PWM_LEFT_PIN | PWM_RIGHT_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWM_PORT, &GPIO_InitStructure);
    
    // 定时器基础配置
    // 【修正】为了匹配控制代码中 0-1000 的占空比范围，ARR 必须设为 999
    // 计算预分频器以获得约 17kHz 频率:
    // Freq = 72MHz / (Prescaler + 1) / (999 + 1) = 17000
    // => (Prescaler + 1) = 72000000 / 1000 / 17000 ≈ 4.23
    // 取 Prescaler = 3 (即 4 分频)，实际频率 = 72M/4/1000 = 18kHz (在允许范围内)
    
    uint16_t prescaler = 3;   // 预分频器设为 3
    uint16_t period = 999;    // 周期设为 999 (对应 0-1000 的分辨率)
    
    TIM_TimeBaseStructure.TIM_Period = period;
    TIM_TimeBaseStructure.TIM_Prescaler = prescaler;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(PWM_TIM, &TIM_TimeBaseStructure);
    
    // PWM模式配置
    
    // --- 【必须加上这一行】 ---
    // 将结构体填入默认值，防止随机垃圾值干扰 TIM1 的高级功能（如互补输出）
    TIM_OCStructInit(&TIM_OCInitStructure);
    
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;  // 初始占空比为0
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    
    // 配置通道1和通道4
    TIM_OC1Init(PWM_TIM, &TIM_OCInitStructure);
    TIM_OC4Init(PWM_TIM, &TIM_OCInitStructure);
    
    // 使能预装载寄存器
    TIM_OC1PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);
    TIM_OC4PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);
    
    // 使能定时器
    TIM_Cmd(PWM_TIM, ENABLE);
    
    // 使能ARR预装载寄存器
    TIM_ARRPreloadConfig(PWM_TIM, ENABLE);
    
    // --- 【关键】开启TIM1主输出使能(MOE) --- 
    // TIM1是高级定时器，必须开启主输出才能真正输出PWM波形
    // 否则PA8/PA11引脚会处于高阻抗状态
    TIM_CtrlPWMOutputs(PWM_TIM, ENABLE);
}

// 设置PWM占空比
void PWM_SetDuty(uint8_t channel, uint16_t duty) {
    // 【核心修复】将最高限幅设置为 900，与mt_pid.c中的设置保持一致
    // 允许电机获得足够的PWM信号来实现反转和急转
    if (duty > 900) duty = 900;
    
    switch (channel) {
        case 0:  // 左电机
            TIM_SetCompare1(PWM_TIM, duty);
            break;
        case 1:  // 右电机
            TIM_SetCompare4(PWM_TIM, duty);
            break;
    }
}