/*
 * mt_track_detect.c
 *
 * Modified: 环岛检测逻辑更新 (4000->3700 且 竖电感约束)
 */

#include "mt_track_detect.h"
#include "main.h"
#include <string.h>
#include <stdlib.h> 
#include "Delay.h"
#include <math.h> 

// 滤波系数
#define FILTER_ALPHA  0.7f

TrackSensor_t g_trackSensor;

// 初始化
void TrackDetect_Init(void) {
    memset(&g_trackSensor, 0, sizeof(TrackSensor_t));
    g_trackSensor.filterWindow = 5;
    g_trackSensor.bufferIndex = 0;
}

// 更新
void TrackDetect_Update(void) {
    // 1. 读取
    for (uint8_t i = 0; i < SENSOR_MAX; i++) {
        g_trackSensor.raw[i] = TrackDetect_ReadADC((SensorChannel_e)i);
    }
    
    // 2. 处理
    TrackDetect_FilterData();
    TrackDetect_NormalizeData();
    TrackDetect_CalculateDeviation(); 
    
    // 3. 特征检测
    g_trackSensor.ringEntryDetected = TrackDetect_CheckRingEntry();
    g_trackSensor.ringExitDetected = TrackDetect_CheckRingExit();
    g_trackSensor.sharpTurnDetected = TrackDetect_CheckSharpTurn();
    
    // 4. 丢线计时
    float total_energy = 0;
    for(int i=0; i<5; i++) total_energy += (float)g_trackSensor.filtered[i];

    if (total_energy < 300) { 
        g_trackSensor.lostLineDuration += 2; 
    } else { 
        g_trackSensor.lostLineDuration = 0; 
    }
}

int16_t TrackDetect_GetDeviation(void) { return g_trackSensor.deviation; }

TrackElementType_e TrackDetect_GetCurrentElement(void) {
    if (g_trackSensor.ringEntryDetected) return TRACK_RING_ENTRY;
    else if (g_trackSensor.ringExitDetected) return TRACK_RING_EXIT;
    else if (g_trackSensor.sharpTurnDetected) return TRACK_SHARP_TURN;
    else return TRACK_NORMAL;
}

// 滤波
void TrackDetect_FilterData(void) {
    static float last_filtered[SENSOR_MAX] = {0}; 
    static uint8_t first_run = 1; 
    
    for (uint8_t i = 0; i < SENSOR_MAX; i++) {
        uint16_t current_raw = g_trackSensor.raw[i];
        if (first_run) last_filtered[i] = (float)current_raw;
        
        float lpf_val = FILTER_ALPHA * (float)current_raw + (1.0f - FILTER_ALPHA) * last_filtered[i];
        last_filtered[i] = lpf_val;
        
        g_trackSensor.filtered[i] = (uint16_t)lpf_val; 
    }
    first_run = 0;
}

// 归一化
void TrackDetect_NormalizeData(void) { 
     uint16_t maxH = 0; 
     uint16_t maxV = 0; 
     
     maxH = g_trackSensor.filtered[SENSOR_L2]; 
     if (g_trackSensor.filtered[SENSOR_M] > maxH) maxH = g_trackSensor.filtered[SENSOR_M]; 
     if (g_trackSensor.filtered[SENSOR_R2] > maxH) maxH = g_trackSensor.filtered[SENSOR_R2]; 
     
     maxV = g_trackSensor.filtered[SENSOR_L1]; 
     if (g_trackSensor.filtered[SENSOR_R1] > maxV) maxV = g_trackSensor.filtered[SENSOR_R1]; 
 
     if (maxH < 100) maxH = 100; 
     if (maxV < 100) maxV = 100; 
 
     for (int i = 0; i < SENSOR_MAX; i++) { 
         float ratio = 0; 
         if (i == SENSOR_L1 || i == SENSOR_R1) { 
             ratio = (float)g_trackSensor.filtered[i] / maxV; 
         } else { 
             ratio = (float)g_trackSensor.filtered[i] / maxH; 
         } 
         if (ratio > 1.0f) ratio = 1.0f;
         g_trackSensor.normalized[i] = (uint16_t)(ratio * 4095.0f); 
     }
     
     g_trackSensor.leftDiff = g_trackSensor.normalized[SENSOR_L1] - g_trackSensor.normalized[SENSOR_L2]; 
     g_trackSensor.rightDiff = g_trackSensor.normalized[SENSOR_R1] - g_trackSensor.normalized[SENSOR_R2]; 
     g_trackSensor.centerValue = g_trackSensor.normalized[SENSOR_M]; 
 }

// 偏差计算 (保持不变，使用 normalized 数据)
void TrackDetect_CalculateDeviation(void) {
    g_trackSensor.lastDeviation = g_trackSensor.deviation;
    
    float L_out = (float)g_trackSensor.normalized[SENSOR_L2]; 
    float L_in  = (float)g_trackSensor.normalized[SENSOR_L1]; 
    float M     = (float)g_trackSensor.normalized[SENSOR_M];  
    float R_in  = (float)g_trackSensor.normalized[SENSOR_R1]; 
    float R_out = (float)g_trackSensor.normalized[SENSOR_R2]; 

    float total_energy = L_out + L_in + M + R_in + R_out;

    if (total_energy < 500) {
        g_trackSensor.deviation = g_trackSensor.lostLineLastValidDev;
        return;
    }

    // 权重
    float K_out = 1.5f;   
    float K_in  = 0.3f;   
    
    float diff = K_out * (L_out - R_out) + K_in * (L_in - R_in);
    float Scale = 1200.0f; 
    float dev = (diff / total_energy) * Scale;

    if (dev > 1500) dev = 1500;
    if (dev < -1500) dev = -1500;
    
    g_trackSensor.deviation = (int16_t)dev;
    
    if (abs(g_trackSensor.deviation) > 20) {
        g_trackSensor.lostLineLastValidDev = g_trackSensor.deviation;
    }
}

// ============================================================
// 【核心修改】环岛入口检测 (直接阈值判断 + 连续计数滤波)
// ============================================================
bool TrackDetect_CheckRingEntry(void) {
    // 1. 获取中间电感值和竖直电感之和 (使用滤波后的 ADC 原始量纲值 0-4095)
    uint16_t center_val = g_trackSensor.filtered[SENSOR_M];
    uint32_t vert_sum = (uint32_t)g_trackSensor.filtered[SENSOR_L1] + (uint32_t)g_trackSensor.filtered[SENSOR_R1];

    // 2. 直接判定逻辑：中间感应极强 且 竖向感应极弱 (典型环岛缺口特征)
    if (center_val > 3000 && vert_sum < 2000) {
        static uint8_t ring_cnt = 0;
        ring_cnt++;
        if (ring_cnt >= 1) { // 连续 2 次检测到，确保信号稳定
            ring_cnt = 0;
            return true;
        }
    } else {
        static uint8_t ring_cnt = 0;
        ring_cnt = 0;
    }

    return false;
}

bool TrackDetect_CheckRingExit(void) {
    if (g_trackSensor.normalized[SENSOR_M] < 800) return true;
    return false;
}

bool TrackDetect_CheckSharpTurn(void) {
    // 1. 获取左右两路竖电感的原始滤波值 (0-4095)
    // 通道对应：SENSOR_L1 为左竖，SENSOR_R1 为右竖
    uint16_t l_vert = g_trackSensor.filtered[SENSOR_L1];
    uint16_t r_vert = g_trackSensor.filtered[SENSOR_R1];

    // 2. 判定逻辑：只要有一端的竖电感强度超过 3000
    // 这代表侧边感应器直接压在了横向黑线上，是典型的直角或急弯特征
    if (l_vert > 2000 || r_vert > 2000) {
        static uint8_t sharp_filter = 0;
        sharp_filter++;
        // 连续 2 次触发确保不是信号跳变（约 10ms）
        if (sharp_filter >= 1) {
            sharp_filter = 0;
            return true;
        }
    } else {
        static uint8_t sharp_filter = 0;
        sharp_filter = 0;
    }

    return false;
}

uint16_t TrackDetect_ReadADC(SensorChannel_e channel) {
    if (channel < SENSOR_MAX) return g_adcBuffer[channel];
    return 0;
}

// 占位
void TrackDetect_ClearFeatureMemory(void) {}
uint8_t TrackDetect_GetContinuousCurveCount(void) { return 0; }
void TrackDetect_AddFeatureToMemory(TrackElementType_e type, int16_t deviation) { (void)type; (void)deviation; }
TrackDetection TrackDetect_GetDetectionResult(void) {
    TrackDetection res = {0};
    res.deviation = g_trackSensor.deviation;
    res.has_detected_ring = g_trackSensor.ringEntryDetected; 
    return res;
}