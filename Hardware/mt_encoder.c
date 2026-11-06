#include "stm32f10x.h"
#include "main.h"
#include "mt_encoder.h"

// 编码器结构体定义
typedef struct {
    int32_t totalCount;     // 累计总脉冲数
    int16_t lastTimerVal;   // 上次定时器的读数
    float speed;            // 速度(cm/s)
} Encoder_t;

// 全局变量
Encoder_t g_encoderLeft;
Encoder_t g_encoderRight;

// 参数定义
#define ENCODER_PULSE_PER_REVOLUTION 1024  // 逐飞1024线
#define WHEEL_RADIUS_CM             3.25f  // 轮子半径(cm)
#define PI                           3.1415926f

// 配置脉冲+方向模式的辅助函数
static void Encoder_Config_PulseDir(TIM_TypeDef* TIMx, uint16_t ClockSource) {
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);
    
    // 【关键修复】使用库文件中正确的宏名称: TIM_TIxExternalCLK1Source_TI2
    TIM_TIxExternalClockConfig(TIMx, ClockSource, TIM_ICPolarity_Rising, 0);
    
    TIM_SetCounter(TIMx, 0);
    TIM_Cmd(TIMx, ENABLE);
}

// 编码器初始化
void Encoder_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    
    // 1. 左轮 (TIM4) - PB7脉冲, PB6方向
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 【关键修复】传入正确的宏参数
    Encoder_Config_PulseDir(TIM4, TIM_TIxExternalCLK1Source_TI2);
    
    // 2. 右轮 (TIM3) - PB5脉冲, PB4方向
    GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 【关键修复】传入正确的宏参数
    Encoder_Config_PulseDir(TIM3, TIM_TIxExternalCLK1Source_TI2);
    
    g_encoderLeft.lastTimerVal = 0;
    g_encoderRight.lastTimerVal = 0;
}

// 核心逻辑：读取计数器变化并结合方向引脚
static void UpdateEncoderSpeed(TIM_TypeDef* TIMx, GPIO_TypeDef* DirPort, uint16_t DirPin, Encoder_t* enc, uint32_t intervalMs) {
    int16_t currentVal = TIM_GetCounter(TIMx);
    int16_t diff = currentVal - enc->lastTimerVal;
    enc->lastTimerVal = currentVal;
    
    // 方向判断：如果电平为低，取反 (如果方向反了，把Bit_RESET改为Bit_SET)
    if (GPIO_ReadInputDataBit(DirPort, DirPin) == Bit_RESET) {
        diff = -diff;
    }
    
    enc->totalCount += diff;
    
    if (intervalMs > 0) {
        float revolutions = (float)diff / ENCODER_PULSE_PER_REVOLUTION;
        float distanceCm = revolutions * 2 * PI * WHEEL_RADIUS_CM;
        enc->speed = distanceCm / (intervalMs / 1000.0f);
    }
}

// 外部调用更新函数
void Encoder_Update(void) {
    static uint32_t lastUpdateTime = 0;
    uint32_t currentTime = GetTick();
    uint32_t interval = currentTime - lastUpdateTime;
    
    if (interval >= 10) {
        UpdateEncoderSpeed(TIM4, GPIOB, GPIO_Pin_6, &g_encoderLeft, interval);
        UpdateEncoderSpeed(TIM3, GPIOB, GPIO_Pin_4, &g_encoderRight, interval);
        lastUpdateTime = currentTime;
    }
}

int32_t Encoder_GetCount(uint8_t encoder) {
    if (encoder == ENCODER_LEFT) return g_encoderLeft.totalCount;
    else return g_encoderRight.totalCount;
}

float Encoder_GetSpeed(uint8_t encoder) {
    if (encoder == ENCODER_LEFT) return g_encoderLeft.speed;
    else return g_encoderRight.speed;
}
