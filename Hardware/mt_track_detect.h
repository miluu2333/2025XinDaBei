/*
 * mt_track_detect.h
 *
 *  Created on: 2025-08-01
 *      Author: XF Team
 */

#ifndef MT_TRACK_DETECT_H_
#define MT_TRACK_DETECT_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// 电感传感器通道定义
typedef enum {
    SENSOR_L2 = 0,    // 最左侧电感
    SENSOR_L1,        // 左侧电感
    SENSOR_M,         // 中间电感
    SENSOR_R1,        // 右侧电感
    SENSOR_R2,        // 最右侧电感
    SENSOR_MAX
} SensorChannel_e;

// 赛道元素类型
typedef enum {
    TRACK_NORMAL,           // 普通赛道
    TRACK_RING_ENTRY,       // 环岛入口
    TRACK_RING_EXIT,        // 环岛出口
    TRACK_SHARP_TURN,       // 直角弯
    TRACK_UNKNOWN           // 未知
} TrackElementType_e;

// 丢线状态枚举
typedef enum {
    LOST_LINE_NONE = 0,     // 未丢线
    LOST_LINE_SLIGHT,       // 轻微丢线
    LOST_LINE_MEDIUM,       // 中等丢线
    LOST_LINE_SEVERE,       // 严重丢线
    LOST_LINE_COMPLETE,     // 完全丢线
} LostLineStatus_e;

// 赛道特征记忆结构
typedef struct {
    TrackElementType_e type;     // 特征类型
    int32_t timestamp;          // 特征出现时间
    int16_t deviation;          // 特征出现时的偏差值
    uint8_t duration;           // 特征持续时间(单位：更新周期)
} TrackFeature_t;

// 赛道特征记忆缓冲区大小
#define TRACK_FEATURE_MEMORY_SIZE 5

// 电感传感器数据结构
typedef struct {
    uint16_t raw[5];           // 原始ADC值
    uint16_t filtered[5];      // 滤波后的值
    uint16_t normalized[5];    // 归一化后的值

    int16_t deviation;         // 赛道偏差值 (-1000 ~ 1000)
    int16_t lastDeviation;     // 上一次偏差值

    // 特征值
    int16_t leftDiff;          // 左侧电感差值
    int16_t rightDiff;         // 右侧电感差值
    int16_t centerValue;       // 中心电感值
    int16_t maxValue;          // 最大电感值
    int16_t minValue;          // 最小电感值

    // 赛道元素检测标志
    bool ringEntryDetected;    // 环岛入口检测标志
    bool ringExitDetected;     // 环岛出口检测标志
    bool sharpTurnDetected;    // 直角弯检测标志

    // 丢线保护相关
    LostLineStatus_e lostLineStatus; // 丢线状态
    uint32_t lostLineStartTime;      // 开始丢线的时间
    uint32_t lostLineDuration;       // 丢线持续时间
    int16_t lostLineLastValidDev;    // 丢线前的最后有效偏差
    bool lostLineRecovered;          // 恢复标志

    // 滤波参数
    uint8_t filterWindow;      // 滑动窗口大小
    uint16_t filterBuffer[SENSOR_MAX][5]; // 滤波缓冲区，窗口大小改为5
    uint8_t bufferIndex;       // 缓冲区索引

    // 赛道特征记忆功能
    TrackFeature_t featureMemory[TRACK_FEATURE_MEMORY_SIZE];  // 特征记忆缓冲区
    uint8_t featureMemoryIndex;       // 当前特征索引
    uint8_t featureMemoryCount;       // 有效特征数量
    TrackElementType_e currentFeature; // 当前赛道特征
    uint32_t currentFeatureStartTime; // 当前特征开始时间
    uint32_t currentFeatureDuration;  // 当前特征持续时间
} TrackSensor_t;

// 赛道检测结果结构体（用于状态机接口）
typedef struct {
    int16_t deviation;              // 赛道偏差值
    bool has_detected_ring;         // 是否检测到环岛
    bool has_detected_obstacle;     // 是否检测到障碍物
    bool is_straight_line;          // 是否为直线
} TrackDetection;

// 外部变量声明
extern TrackSensor_t g_trackSensor;
// ADC DMA 缓冲（由 main.c 提供）
extern volatile uint16_t g_adcBuffer[SENSOR_MAX];

// 函数声明
void TrackDetect_Init(void);
void TrackDetect_Update(void);
int16_t TrackDetect_GetDeviation(void);
TrackElementType_e TrackDetect_GetCurrentElement(void);

// 数据处理函数
void TrackDetect_FilterData(void);
void TrackDetect_NormalizeData(void);
void TrackDetect_CalculateDeviation(void);

// 特征检测函数
bool TrackDetect_CheckRingEntry(void);
bool TrackDetect_CheckRingExit(void);
bool TrackDetect_CheckSharpTurn(void);

// 赛道特征记忆功能
void TrackDetect_ClearFeatureMemory(void);      // 清除赛道特征记忆
uint8_t TrackDetect_GetContinuousCurveCount(void);  // 获取连续弯道数量

// 获取赛道检测结果函数（用于状态机接口）
TrackDetection TrackDetect_GetDetectionResult(void);

// 读取指定通道的ADC值
uint16_t TrackDetect_ReadADC(SensorChannel_e channel);

// 差比和计算函数
extern int32_t Cha_BI_He_Cha(uint16_t L, uint16_t R, uint16_t LM, uint16_t RM, float A, float B, uint8_t shift);

#endif /* MT_TRACK_DETECT_H_ */
