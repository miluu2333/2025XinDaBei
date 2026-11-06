/*
 * mt_vofa.h
 *
 *  Created on: 2025-08-01
 *      Author: XF Team
 */

#ifndef MT_VOFA_H_
#define MT_VOFA_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"
#include "mt_track_detect.h"
#include "mt_pid.h"
#include "mt_encoder.h"
#include "mt_mpu6050.h"
#include "mt_ultrasonic.h"
#include "mt_state_machine.h"

// VOFA+ 通信协议类型
typedef enum {
    PROTOCOL_RAW = 0,       // 原始数据格式
    PROTOCOL_JUST_FLOAT,    // JustFloat 协议
    PROTOCOL_FREEMASTER     // FreeMASTER 协议
} VofaProtocol_e;

// VOFA+ 数据结构
typedef struct {
    VofaProtocol_e protocol;        // 通信协议
    
    // 发送缓冲区
    uint8_t txBuffer[128];          // 发送缓冲区
    uint16_t txBufferSize;          // 缓冲区大小
    
    // 数据帧格式
    uint8_t frameHeader[4];         // 帧头
    uint8_t frameTail[4];           // 帧尾
    
    // 发送状态
    bool isSending;                 // 发送中标志
    uint32_t lastSendTime;          // 上一次发送时间
    uint32_t sendInterval;          // 发送间隔 (ms)
    
    // 使能标志
    bool enable;                    // 使能标志
} Vofa_t;

// 遥测数据结构
typedef struct {
    // 传感器数据
    int16_t trackDeviation;         // 赛道偏差
    uint16_t adcValues[5];          // 电感ADC值
    
    // 控制数据
    float targetSpeed;              // 目标速度
    float currentSpeed;             // 当前速度
    float leftPWM;                  // 左电机PWM
    float rightPWM;                 // 右电机PWM
    
    // 姿态数据
    float yawAngle;                 // 偏航角
    float pitchAngle;               // 俯仰角
    float rollAngle;                // 横滚角
    
    // 状态标志
    uint8_t vehicleState;           // 车辆状态
    uint8_t trackElement;           // 赛道元素类型
    
    // 超声波数据
    float obstacleDistance;         // 障碍物距离
} TelemetryData_t;

// 外部变量声明
extern Vofa_t g_vofa;
extern TelemetryData_t g_telemetryData;

// 函数声明
void Vofa_Init(void);
void Vofa_SetProtocol(VofaProtocol_e protocol);
void Vofa_SetSendInterval(uint32_t interval);
void Vofa_Enable(bool enable);
void Vofa_Update(void);
void Vofa_SendData(void);

// 数据打包函数
void Vofa_PackJustFloat(void);
void Vofa_PackRawData(void);

// 遥测数据更新函数
void Telemetry_Update(void);
void Telemetry_Reset(void);

#endif /* MT_VOFA_H_ */
