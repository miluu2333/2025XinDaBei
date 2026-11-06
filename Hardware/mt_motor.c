#include "stm32f10x.h" 
#include "main.h" 
#include "mt_motor.h" 
#include "pwm_config.h" 
#include "Delay.h" 
#include "mt_track_detect.h" 
#include <stdlib.h> // 引入 abs 函数 
 
/* TIM1 的 ARR = 999，占空比量程 0~1000。 */
#define PWM_MAX_DUTY  1000

Motor_t g_motor; 
 
// static int16_t encoder_data_1[16] = {0};
// static int16_t encoder_data_2[16] = {0};
// static float difSpeed[3] = {0};
static int16_t RAllPulse = 0; 
 
void Motor_Init(void) 
{ 
    GPIO_InitTypeDef GPIO_InitStructure; 
    
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE); 
    
    // 左电机方向引脚 
    GPIO_InitStructure.GPIO_Pin = MOTOR_LEFT_DIR_PIN; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_Init(MOTOR_LEFT_DIR_PORT, &GPIO_InitStructure); 
    
    // 右电机方向引脚 
    GPIO_InitStructure.GPIO_Pin = MOTOR_RIGHT_DIR_PIN; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
    GPIO_Init(MOTOR_RIGHT_DIR_PORT, &GPIO_InitStructure); 
    
    PWM_Init(); 
    
    g_motor.leftDuty = 0; 
    g_motor.rightDuty = 0; 
    g_motor.leftDir = 1; 
    g_motor.rightDir = 1; 
    g_motor.targetSpeed = 0.0f; 
    g_motor.currentSpeed = 0.0f; 
    g_motor.differentSpeed = 0; 
    g_motor.totalPulses = 0; 
    
    Motor_Stop(); 
} 
 
void Motor_Turn(int32_t leftDuty, int32_t rightDuty) {
    /* 越界保护：避免调参时占空比超过定时器量程。 */
    if (leftDuty  >  PWM_MAX_DUTY) leftDuty  =  PWM_MAX_DUTY;
    if (leftDuty  < -PWM_MAX_DUTY) leftDuty  = -PWM_MAX_DUTY;
    if (rightDuty >  PWM_MAX_DUTY) rightDuty =  PWM_MAX_DUTY;
    if (rightDuty < -PWM_MAX_DUTY) rightDuty = -PWM_MAX_DUTY;

    // --- 左轮 (PB12 控制方向) ---
    if (leftDuty >= 0) {
       GPIO_SetBits(GPIOB, GPIO_Pin_12);   // 1为正向
       PWM_SetDuty(0, (uint16_t)leftDuty);
    } else {
       GPIO_ResetBits(GPIOB, GPIO_Pin_12); // 0为反向 
       PWM_SetDuty(0, (uint16_t)(-leftDuty)); // 取绝对值传给PWM
    }

    // --- 右轮 (PB13 控制方向) ---
    if (rightDuty >= 0) {
       GPIO_SetBits(GPIOB, GPIO_Pin_13);
       PWM_SetDuty(1, (uint16_t)rightDuty);
    } else {
       GPIO_ResetBits(GPIOB, GPIO_Pin_13);
       PWM_SetDuty(1, (uint16_t)(-rightDuty));
    }
} 
 
/* 常用运动封装 */ 
void Motor_Foreward(uint16_t pwm) { Motor_Turn(pwm, pwm); } 
void Motor_Backward(uint16_t pwm) { Motor_Turn(-pwm, -pwm); } 
void Motor_TurnLeft(uint16_t pwm) { Motor_Turn(-pwm, pwm); } 
void Motor_TurnRight(uint16_t pwm) { Motor_Turn(pwm, -pwm); } 
void Motor_Stop(void) { Motor_Turn(0, 0); } 
 
void Motor_EncoderIntegral(void) { 
    RAllPulse += g_motor.currentSpeed; 
    g_motor.totalPulses = RAllPulse; 
} 
 
Motor_t* Motor_GetState(void) { return &g_motor; } 
void Motor_SetTargetSpeed(float speed) { g_motor.targetSpeed = speed; } 
float Motor_GetTargetSpeed(void) { return g_motor.targetSpeed; } 
void Motor_SetCurrentSpeed(float speed) { g_motor.currentSpeed = speed; } 
float Motor_GetCurrentSpeed(void) { return g_motor.currentSpeed; } 
void Motor_SetDifferentSpeed(int16_t speed) { g_motor.differentSpeed = speed; } 
int16_t Motor_GetDifferentSpeed(void) { return g_motor.differentSpeed; } 
int16_t Motor_GetTotalPulses(void) { return g_motor.totalPulses; } 
void Motor_ResetTotalPulses(void) { RAllPulse = 0; g_motor.totalPulses = 0; }
