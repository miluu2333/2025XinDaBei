#ifndef MT_MODE_CONTROL_H_
#define MT_MODE_CONTROL_H_

#include "main.h"

// 运行模式枚举
typedef enum {
    MODE_STOP = 0,      // 模式1: 停止模式 (上电默认)
    MODE_DEBUG,         // 模式2: 调试模式 (电机不转，只看波形)
    MODE_TRACKING,      // 模式3: 循迹模式 (电机转动，全速运行)
    MODE_EMERGENCY,     // 急停模式 (保留)
    MODE_NUM            // 模式总数
} RUN_MODE;

// OLED菜单枚举 (保留)
typedef enum {
    MENU_STATUS = 0,
    MENU_PID,
    MENU_SENSOR,
    MENU_NUM
} OLED_MENU;

// 系统模式状态结构体
typedef struct {
    RUN_MODE current_mode;      // 当前模式
    RUN_MODE last_mode;         // 上一次模式
    OLED_MENU current_menu;     // 当前菜单
    uint8_t mode_changed;       // 模式改变标志
    uint32_t mode_switch_time;  // 模式切换时间

    // 按键状态
    uint8_t key1_click_count;
    uint32_t key1_last_click;

    // 紧急停止
    uint8_t emergency_active;
    uint32_t emergency_time;
} SYSTEM_MODE_STATE;

extern SYSTEM_MODE_STATE g_mode_state;

// 函数声明
void ModeControl_Init(void);
void ModeControl_Execute(void);
void ModeControl_Key1_Handle(uint8_t key_state);
void ModeControl_Key2_Handle(uint8_t key_state);
RUN_MODE ModeControl_GetMode(void);
void ModeControl_SetMode(RUN_MODE new_mode);
void ModeControl_EmergencyStop(void);
void ModeControl_ClearEmergency(void);

#endif /* MT_MODE_CONTROL_H_ */
