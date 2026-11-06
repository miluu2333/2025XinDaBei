/*
 * mt_mpu6050.h
 *
 *  Created on: 2025-08-01
 *      Author: XF Team
 */

#ifndef MT_MPU6050_H_
#define MT_MPU6050_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// MPU6050 设备地址
#define MPU6050_ADDR_AD0_LOW     0x68    // AD0 = 0
#define MPU6050_ADDR_AD0_HIGH    0x69    // AD0 = 1
#define MPU6050_ADDR             MPU6050_ADDR_AD0_LOW

// MPU6050 寄存器定义
#define MPU6050_REG_SMPLRT_DIV   0x19    // 采样率分频器
#define MPU6050_REG_CONFIG       0x1A    // 配置寄存器
#define MPU6050_REG_GYRO_CONFIG  0x1B    // 陀螺仪配置
#define MPU6050_REG_ACCEL_CONFIG 0x1C    // 加速度计配置
#define MPU6050_REG_ACCEL_XOUT_H 0x3B    // 加速度计X轴高位
#define MPU6050_REG_ACCEL_XOUT_L 0x3C    // 加速度计X轴低位
#define MPU6050_REG_ACCEL_YOUT_H 0x3D    // 加速度计Y轴高位
#define MPU6050_REG_ACCEL_YOUT_L 0x3E    // 加速度计Y轴低位
#define MPU6050_REG_ACCEL_ZOUT_H 0x3F    // 加速度计Z轴高位
#define MPU6050_REG_ACCEL_ZOUT_L 0x40    // 加速度计Z轴低位
#define MPU6050_REG_TEMP_OUT_H   0x41    // 温度传感器高位
#define MPU6050_REG_TEMP_OUT_L   0x42    // 温度传感器低位
#define MPU6050_REG_GYRO_XOUT_H  0x43    // 陀螺仪X轴高位
#define MPU6050_REG_GYRO_XOUT_L  0x44    // 陀螺仪X轴低位
#define MPU6050_REG_GYRO_YOUT_H  0x45    // 陀螺仪Y轴高位
#define MPU6050_REG_GYRO_YOUT_L  0x46    // 陀螺仪Y轴低位
#define MPU6050_REG_GYRO_ZOUT_H  0x47    // 陀螺仪Z轴高位
#define MPU6050_REG_GYRO_ZOUT_L  0x48    // 陀螺仪Z轴低位
#define MPU6050_REG_PWR_MGMT_1   0x6B    // 电源管理1
#define MPU6050_REG_PWR_MGMT_2   0x6C    // 电源管理2
#define MPU6050_REG_WHO_AM_I     0x75    // 设备ID

// 陀螺仪量程
typedef enum {
    GYRO_FS_250DPS = 0,    // ±250°/s
    GYRO_FS_500DPS,        // ±500°/s
    GYRO_FS_1000DPS,       // ±1000°/s
    GYRO_FS_2000DPS        // ±2000°/s
} GyroFullScale_e;

// 加速度计量程
typedef enum {
    ACCEL_FS_2G = 0,       // ±2g
    ACCEL_FS_4G,           // ±4g
    ACCEL_FS_8G,           // ±8g
    ACCEL_FS_16G           // ±16g
} AccelFullScale_e;

// MPU6050 数据结构
typedef struct {
    // 原始数据
    int16_t accelRaw[3];     // 加速度计原始数据 [X, Y, Z]
    int16_t gyroRaw[3];      // 陀螺仪原始数据 [X, Y, Z]
    int16_t tempRaw;         // 温度传感器原始数据

    // 转换后的数据
    float accelG[3];         // 加速度计数据 (g)
    float gyroDPS[3];        // 陀螺仪数据 (°/s)
    float temperature;       // 温度 (°C)

    // 姿态数据
    float yaw;               // 偏航角 (°)
    float pitch;             // 俯仰角 (°)
    float roll;              // 横滚角 (°)

    // 配置参数
    GyroFullScale_e gyroFS;  // 陀螺仪量程
    AccelFullScale_e accelFS;// 加速度计量程
    float gyroSensitivity;   // 陀螺仪灵敏度 (°/s/LSB)
    float accelSensitivity;  // 加速度计灵敏度 (g/LSB)

    // 校准参数
    float gyroBias[3];       // 陀螺仪零偏
    float accelBias[3];      // 加速度计零偏

    // 滤波参数
    float complementaryAlpha; // 互补滤波系数
    bool filterEnable;       // 滤波使能标志

    // 初始化标志
    bool initialized;        // 初始化完成标志
    bool calibrationDone;    // 校准完成标志
} MPU6050_t;

// 外部变量声明
extern MPU6050_t g_mpu6050;

// 函数声明
bool MPU6050_Init(void);
bool MPU6050_TestConnection(void);
void MPU6050_SetGyroFullScale(GyroFullScale_e fs);
void MPU6050_SetAccelFullScale(AccelFullScale_e fs);
void MPU6050_SetSampleRateDivider(uint8_t divider);
void MPU6050_SetFilterBandwidth(uint8_t bandwidth);
void MPU6050_Update(void);
void MPU6050_GetRawData(int16_t* accel, int16_t* gyro, int16_t* temp);
void MPU6050_GetProcessedData(float* accelG, float* gyroDPS, float* temp);
void MPU6050_GetEulerAngles(float* yaw, float* pitch, float* roll);
void MPU6050_SetYawOffset(float offset);
void MPU6050_ResetYaw(void);

// 校准函数
bool MPU6050_CalibrateGyro(uint16_t samples);
bool MPU6050_CalibrateAccel(uint16_t samples);

// 姿态更新函数
void MPU6050_UpdateEulerAngles(void);

#endif /* MT_MPU6050_H_ */
