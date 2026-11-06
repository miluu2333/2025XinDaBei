#ifndef MT_PID_H_
#define MT_PID_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// ================================================================
// 简化后的单环 PID 结构
// ================================================================

// 舵机/转向 PID 参数结构体
typedef struct {
    // 保留有效的转向参数
    float outer_kp;         // 外环 P
    float outer_kd;         // 外环 D
    float feedforward;      // 暴力前馈

    // 保留有效的角度锁参数
    float angle_kp;         // 角度环比例 (用于环岛打角)
    float angle_kd;         // 角度环微分

    // --- 以下变量已删除：turn_kp, turn_kd, speed_max, speed_ff ---
} SteeringPID_Config_t;

extern volatile SteeringPID_Config_t g_pidConfig;

// 电机控制器状态结构体
typedef struct {
    int16_t leftPWM;
    int16_t rightPWM;
    float targetSpeed;
    float currentSpeed;
    float leftSpeedFeedback;
    float rightSpeedFeedback;

    // --- 以下变量已删除：targetAngle, currentAngle (VOFA不显示，逻辑不用) ---
} MotorController_t;

extern MotorController_t g_motorController;

// 全局控制变量
extern float v_target_l;
extern float v_target_r;

// ================================================================
// 函数接口
// ================================================================
void MotorControl_Init(void);
void MotorControl_Update(void);
void Track_Line_Control(void);
void Yaw_control_200hz(int target_angle);
void MotorControl_SetSpeed(float left, float right);

#endif /* MT_PID_H_ */