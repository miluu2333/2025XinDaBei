/*
 * mt_vofa.c
 *
 * Created on: 2025-08-01
 * Author: XF Team
 * Modified: 优化电感数据遥测，确保5路电感值正确发送
 */

#include "mt_vofa.h"
#include "mt_track_detect.h"
#include "mt_pid.h"
#include "mt_encoder.h"
#include "mt_mpu6050.h"
#include "mt_ultrasonic.h"
#include "mt_state_machine.h"
#include "main.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h> // 确保包含 sprintf
#include "Delay.h" 
// 全局变量定义
Vofa_t g_vofa;
TelemetryData_t g_telemetryData;

// JustFloat协议帧头帧尾
#define JUST_FLOAT_HEADER 0x00 
#define JUST_FLOAT_TAIL   {0x00, 0x00, 0x80, 0x7F} 

// 初始化VOFA+模块
void Vofa_Init(void) {
    memset(&g_vofa, 0, sizeof(Vofa_t));
    memset(&g_telemetryData, 0, sizeof(TelemetryData_t));
    
    // 设置默认协议为JustFloat
    g_vofa.protocol = PROTOCOL_JUST_FLOAT;
    
    // 设置默认发送间隔为 20ms (50Hz)        
    g_vofa.sendInterval = 20;
    g_vofa.lastSendTime = GetTick();
    
    g_vofa.enable = true;
    g_vofa.isSending = false;
}

void Vofa_SetProtocol(VofaProtocol_e protocol) { g_vofa.protocol = protocol; }
void Vofa_SetSendInterval(uint32_t interval) { g_vofa.sendInterval = interval; }
void Vofa_Enable(bool enable) { g_vofa.enable = enable; }

// 更新VOFA+
void Vofa_Update(void) {
    if (!g_vofa.enable) return;
    
    // 检查是否需要发送数据
    uint32_t currentTime = GetTick();
    if ((currentTime - g_vofa.lastSendTime) >= g_vofa.sendInterval) {
        Vofa_SendData();
        g_vofa.lastSendTime = currentTime;
    }
}

// 发送数据
void Vofa_SendData(void) {
    if (g_vofa.isSending) return;
    
    g_vofa.isSending = true;
    
    switch (g_vofa.protocol) {
        case PROTOCOL_JUST_FLOAT:
            Vofa_PackJustFloat();
            break;
        case PROTOCOL_RAW:
            Vofa_PackRawData();
            break;
        default:
            break;
    }
    
    if (g_vofa.txBufferSize > 0) {
        for (uint16_t i = 0; i < g_vofa.txBufferSize; i++) {
            while (USART_GetFlagStatus(UART, USART_FLAG_TXE) == RESET);
            USART_SendData(UART, g_vofa.txBuffer[i]);
        }
    }
    
    g_vofa.isSending = false;
}

// 打包JustFloat格式数据
void Vofa_PackJustFloat(void) {
    uint16_t index = 0;
    
    // 1. 赛道偏差 (Channel 0)
    float dev = (float)g_telemetryData.trackDeviation;
    memcpy(&g_vofa.txBuffer[index], &dev, sizeof(float));
    index += sizeof(float);
    
    // 2. 电感ADC值 (Channel 1-5)
    for (uint8_t i = 0; i < 5; i++) {
        float adcValue = (float)g_telemetryData.adcValues[i];
        memcpy(&g_vofa.txBuffer[index], &adcValue, sizeof(float));
        index += sizeof(float);
    }
    
    // 3. 速度数据 (Channel 6-7)
    memcpy(&g_vofa.txBuffer[index], &g_telemetryData.targetSpeed, sizeof(float));
    index += sizeof(float);
    memcpy(&g_vofa.txBuffer[index], &g_telemetryData.currentSpeed, sizeof(float));
    index += sizeof(float);
    
    // 4. 姿态数据 (Channel 8: Yaw)
    memcpy(&g_vofa.txBuffer[index], &g_telemetryData.yawAngle, sizeof(float));
    index += sizeof(float);
    
    // 5. 陀螺仪原始 Z 轴角速度 (Channel 9) - 用于调试
    float gyroZ = g_mpu6050.gyroDPS[2];
    memcpy(&g_vofa.txBuffer[index], &gyroZ, sizeof(float));
    index += sizeof(float);
    
    // 6. 超声波距离数据 (Channel 10)
    float dist = g_telemetryData.obstacleDistance;
    memcpy(&g_vofa.txBuffer[index], &dist, sizeof(float));
    index += sizeof(float);
    
    // 添加 JustFloat 帧尾
    uint8_t tail[] = JUST_FLOAT_TAIL;
    memcpy(&g_vofa.txBuffer[index], tail, sizeof(tail));
    index += sizeof(tail);
    
    g_vofa.txBufferSize = index;
}

// 打包原始数据格式 (CSV)
void Vofa_PackRawData(void) {
    uint16_t index = 0;
    index += sprintf((char*)&g_vofa.txBuffer[index], "%d,%d,%d,%d,%d,%d,%.2f\r\n",
                    g_telemetryData.trackDeviation,
                    g_telemetryData.adcValues[0], // L2
                    g_telemetryData.adcValues[1], // L1
                    g_telemetryData.adcValues[2], // M
                    g_telemetryData.adcValues[3], // R1
                    g_telemetryData.adcValues[4], // R2
                    g_telemetryData.yawAngle);
    g_vofa.txBufferSize = index;
}

// 更新遥测数据
void Telemetry_Update(void) {
    // 1. 更新赛道偏差
    g_telemetryData.trackDeviation = TrackDetect_GetDeviation();
    
    // 2. 更新电感ADC值 (改为发送原始值 raw，方便硬件调试)
    for (uint8_t i = 0; i < 5; i++) {
        g_telemetryData.adcValues[i] = g_trackSensor.raw[i]; 
    }
    
    // 3. 更新速度数据
    g_telemetryData.targetSpeed = g_motorController.targetSpeed;
    g_telemetryData.currentSpeed = g_motorController.currentSpeed;
    
    // 4. 更新PWM数据
    g_telemetryData.leftPWM = (float)g_motorController.leftPWM;
    g_telemetryData.rightPWM = (float)g_motorController.rightPWM;
    
    // 5. 更新姿态数据
    g_telemetryData.yawAngle = g_mpu6050.yaw;
    g_telemetryData.pitchAngle = g_mpu6050.pitch;
    g_telemetryData.rollAngle = g_mpu6050.roll;
    
    // 6. 更新状态标志
    g_telemetryData.vehicleState = (uint8_t)g_stateMachine.currentState;
    g_telemetryData.trackElement = (uint8_t)TrackDetect_GetCurrentElement();
    
    // 7. 更新超声波数据
    g_telemetryData.obstacleDistance = g_ultrasonic.distance;
}

// 重置遥测数据
void Telemetry_Reset(void) {
    memset(&g_telemetryData, 0, sizeof(TelemetryData_t));
}
