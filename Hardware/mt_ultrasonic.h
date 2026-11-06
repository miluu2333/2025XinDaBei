/*
 * mt_ultrasonic.h
 *
 *  Created on: 2025-08-01
 *      Author: XF Team
 */

#ifndef MT_ULTRASONIC_H_
#define MT_ULTRASONIC_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// 超声波传感器状态
typedef enum {
    ULTRASONIC_IDLE = 0,      // 空闲状态
    ULTRASONIC_TRIGGERING,    // 触发中
    ULTRASONIC_MEASURING,     // 测量中
    ULTRASONIC_DONE,          // 测量完成
    ULTRASONIC_ERROR          // 错误状态
} UltrasonicState_e;

// 超声波传感器数据结构
typedef struct {
    // 硬件参数
    uint32_t triggerPin;      // 触发引脚
    uint32_t echoPin;         // 回波引脚
    GPIO_TypeDef* triggerPort;// 触发端口
    GPIO_TypeDef* echoPort;   // 回波端口

    // 测量参数
    float distance;           // 测量距离 (cm)
    float lastDistance;       // 上一次测量距离
    uint32_t measureStartTime;// 测量开始时间
    uint32_t measureEndTime;  // 测量结束时间
    uint32_t pulseWidth;      // 回波脉冲宽度 (us)

    // 配置参数
    uint32_t maxDistance;     // 最大测量距离 (cm)
    uint32_t minDistance;     // 最小测量距离 (cm)
    uint32_t measureInterval; // 测量间隔 (ms)
    uint32_t timeout;         // 超时时间 (us)

    // 状态参数
    UltrasonicState_e state;  // 当前状态
    bool enable;              // 使能标志
    bool obstacleDetected;    // 障碍物检测标志
    uint32_t lastMeasureTime; // 上一次测量时间

    // 滤波参数
    float filterAlpha;        // 低通滤波系数
    bool filterEnable;        // 滤波使能标志

    // 障碍物检测参数
    float obstacleThreshold;  // 障碍物检测阈值 (cm)
    uint8_t obstacleDebounce; // 障碍物检测防抖次数
    uint8_t obstacleCount;    // 障碍物检测计数
} Ultrasonic_t;

// 外部变量声明
extern volatile Ultrasonic_t g_ultrasonic;

// 函数声明
void Ultrasonic_Init(void);
void Ultrasonic_SetPins(GPIO_TypeDef* triggerPort, uint32_t triggerPin,
                       GPIO_TypeDef* echoPort, uint32_t echoPin);
void Ultrasonic_SetParameters(uint32_t minDistance, uint32_t maxDistance,
                             uint32_t measureInterval);
void Ultrasonic_SetObstacleThreshold(float threshold);
void Ultrasonic_Enable(bool enable);
void Ultrasonic_Update(void);
float Ultrasonic_GetDistance(void);
bool Ultrasonic_IsObstacleDetected(void);
void Ultrasonic_TriggerMeasurement(void);
bool Ultrasonic_IsMeasurementReady(void);

// 中断处理函数
void Ultrasonic_EchoCallback(void);

#endif /* MT_ULTRASONIC_H_ */
