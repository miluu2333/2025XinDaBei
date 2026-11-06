#include "mt_mode_control.h"
#include "Key.h"
#include "mt_motor.h"
#include "main.h"
#include "Delay.h"

// 蜂鸣器控制函数声明
extern void Buzzer_Beep(uint32_t duration_ms);
extern void Buzzer_Tone(uint16_t frequency, uint32_t duration_ms);

// 全局变量
SYSTEM_MODE_STATE g_mode_state;

/**
 * 模式控制初始化
 */
void ModeControl_Init(void)
{
    // 上电默认进入停止模式
    g_mode_state.current_mode = MODE_STOP;
    g_mode_state.last_mode = MODE_STOP;
    g_mode_state.current_menu = MENU_STATUS;
    g_mode_state.mode_changed = 1;
    g_mode_state.emergency_active = 0;
}

/**
 * 按键1处理函数：切换模式
 * 顺序: 停止 -> 调试 -> 循迹 -> 停止...
 */
void ModeControl_Key1_Handle(uint8_t key_state)
{
    if (key_state == KEY_CLICK)
    {
        RUN_MODE next_mode = g_mode_state.current_mode;

        // 简单的状态机切换逻辑
        switch (g_mode_state.current_mode) {
            case MODE_STOP:
                next_mode = MODE_DEBUG;     // 1 -> 2
                break;
            case MODE_DEBUG:
                next_mode = MODE_TRACKING;  // 2 -> 3
                break;
            case MODE_TRACKING:
                next_mode = MODE_STOP;      // 3 -> 1
                break;
            default:
                next_mode = MODE_STOP;
                break;
        }

        // 如果处于急停状态，任何按键操作都先解除急停，回到停止模式
        if (g_mode_state.emergency_active) {
            ModeControl_ClearEmergency();
            return;
        }

        ModeControl_SetMode(next_mode);
    }
    // 长按: 紧急停止
    else if (key_state == KEY_LONG_PRESS)
    {
        ModeControl_EmergencyStop();
    }
}

/**
 * 按键2处理函数 (备用，可用于其他功能)
 */
void ModeControl_Key2_Handle(uint8_t key_state)
{
    if (key_state == KEY_CLICK)
    {
        // 示例：切换菜单或其他辅助功能
        Buzzer_Tone(600, 50);
    }
}

/**
 * 模式控制执行函数 (在主循环中调用)
 * 负责执行每个模式下的"持续性"动作
 */
void ModeControl_Execute(void)
{
    // 急停优先处理
    if (g_mode_state.emergency_active)
    {
        Motor_Stop();
        return;
    }

    switch (g_mode_state.current_mode)
    {
        case MODE_STOP:
            // 模式1: 停止模式
            // 动作: 强制锁死电机
            Motor_Stop();
            break;

        case MODE_DEBUG:
            // 模式2: 调试模式
            // 动作:
            /* 强制锁死电机，防止误动作 */
            Motor_Stop();

            // 传感器数据更新和VOFA发送由 main.c 统一处理，这里不用写
            // (只要不在 STOP 模式把系统时钟关了就行)
            break;

        case MODE_TRACKING:
            // 模式3: 循迹模式
            // 动作:
            // 1. 这里的电机控制逻辑实际上是在 main.c 的 PID 定时器里执行的
            // 2. 这里只需要确保不发送 Motor_Stop() 即可
            break;

        default:
            Motor_Stop();
            break;
    }
}

/**
 * 设置运行模式
 */
void ModeControl_SetMode(RUN_MODE new_mode)
{
    if (g_mode_state.current_mode == new_mode) return;

    g_mode_state.last_mode = g_mode_state.current_mode;
    g_mode_state.current_mode = new_mode;
    g_mode_state.mode_changed = 1;
    g_mode_state.mode_switch_time = GetTick();

    // 模式切换提示音 (通过声音区分模式)
    switch (new_mode)
    {
        case MODE_STOP:
            // 响一声: 滴
            Buzzer_Tone(1000, 100);
            Motor_Stop(); // 立即停车
            break;

        case MODE_DEBUG:
            // 响两声: 滴-滴
            Buzzer_Tone(1000, 100);
            Delay_ms(100);
            Buzzer_Tone(1000, 100);
            /* 强制停车，不再记录调试目标角度 */
            Motor_Stop();
            break;

        case MODE_TRACKING:
            // 响一长声: 滴~~~~ (准备起飞)
            Buzzer_Tone(2000, 400);
            // 这里不调用 Motor_Stop，交给 PID 接管
            break;

        default:
            Buzzer_Beep(50);
            break;
    }
}

void ModeControl_EmergencyStop(void)
{
    g_mode_state.emergency_active = 1;
    ModeControl_SetMode(MODE_STOP); // 切回停止模式
    Motor_Stop();

    for(int i=0; i<3; i++) {
        Buzzer_Tone(3000, 100);
        Delay_ms(100);
    }
}

void ModeControl_ClearEmergency(void)
{
    g_mode_state.emergency_active = 0;
    ModeControl_SetMode(MODE_STOP); // 恢复到停止状态
    Buzzer_Tone(1000, 200);
}

RUN_MODE ModeControl_GetMode(void)
{
    return g_mode_state.current_mode;
}
