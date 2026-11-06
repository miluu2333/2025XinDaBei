#ifndef MT_MOTOR_H_
#define MT_MOTOR_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// 电机结构体
typedef struct {
    int16_t leftDuty;         // 左电机PWM占空比
    int16_t rightDuty;        // 右电机PWM占空比
    int16_t leftDir;          // 左电机方向 (1: 正转, -1: 反转)
    int16_t rightDir;         // 右电机方向 (1: 正转, -1: 反转)

    float targetSpeed;        // 目标速度 (cm/s)
    float currentSpeed;       // 当前速度 (cm/s)
    int16_t differentSpeed;   // 差速值
    int16_t totalPulses;      // 总脉冲数
} Motor_t;

// 电机硬件配置
// 左电机方向引脚 (修改为 PB12)
#define MOTOR_LEFT_DIR_PORT     GPIOB
#define MOTOR_LEFT_DIR_PIN      GPIO_Pin_12

// 右电机方向引脚 (修改为 PB13)
#define MOTOR_RIGHT_DIR_PORT    GPIOB
#define MOTOR_RIGHT_DIR_PIN     GPIO_Pin_13

// 外部变量声明
extern Motor_t g_motor;

// 函数声明
void Motor_Init(void);
void Motor_Turn(int32_t leftDuty, int32_t rightDuty);
void Motor_Foreward(uint16_t pwm);
void Motor_Backward(uint16_t pwm);
void Motor_TurnLeft(uint16_t pwm);
void Motor_TurnRight(uint16_t pwm);
void Motor_Stop(void);
void Motor_EncoderIntegral(void);

Motor_t* Motor_GetState(void);
void Motor_SetTargetSpeed(float speed);
float Motor_GetTargetSpeed(void);
void Motor_SetCurrentSpeed(float speed);
float Motor_GetCurrentSpeed(void);
void Motor_SetDifferentSpeed(int16_t speed);
int16_t Motor_GetDifferentSpeed(void);
int16_t Motor_GetTotalPulses(void);
void Motor_ResetTotalPulses(void);

#endif /* MT_MOTOR_H_ */
