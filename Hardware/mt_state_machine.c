#include "mt_state_machine.h"
#include "mt_pid.h"
#include "mt_track_detect.h"
#include "mt_mpu6050.h"
#include "mt_motor.h"
#include "mt_ultrasonic.h"
#include "mt_mode_control.h"
#include "Delay.h"
#include <math.h>
#include <stdlib.h>

// 外部变量引用
extern float v_target_l;
extern float v_target_r;
extern int setup_speed_l;
extern int setup_speed_r;

// 外部函数声明
extern void Track_Line_Control(void);
extern void Yaw_control_200hz(int target);
extern void Buzzer_Beep(uint32_t duration_ms);

StateMachine_t g_stateMachine;

// ====================================================================
// 本地静态变量
// ====================================================================
static uint8_t s_distCheckCount = 0; // 距离检测滤波计数
static uint8_t s_avoidPassCount = 0;  /* 已完成的避障次数 */

// ====================================================================
// 参数配置
// ====================================================================
#define RING_ENTRY_ANGLE      30.0f
#define OBS_DETECT_DIST       60.0f   // 检测距离 (cm)
#define OBS_PHASE_TIMEOUT_MS   2000   /* 【优化13】避障相位超时（ms） */

// 辅助函数
uint32_t GetPhaseDuration(void) { return GetTick() - g_stateMachine.obsPhaseTime; }

// ====================================================================
// 0. 初始化
// ====================================================================
void StateMachine_Init(void) {
    g_stateMachine.currentState = STATE_STRAIGHT;
    g_stateMachine.lastState = STATE_STRAIGHT;
    g_stateMachine.stateEnterTime = GetTick();
    g_stateMachine.lockStatus = false;

    s_distCheckCount = 0;
    s_avoidPassCount = 0; // 上电清零
}

// ====================================================================
// 状态机调度
// ====================================================================
void StateMachine_Update(void) {
    switch (g_stateMachine.currentState) {
        case STATE_STRAIGHT:    State_HandleStraight();    break;
        case STATE_RING:        State_HandleRing();        break;
        case STATE_SHARP_TURN:  State_HandleSharpTurn();   break;
        case STATE_OBSTACLE:    State_HandleObstacle();    break;
        case STATE_RIGHT_BLIND: State_HandleRightBlind();  break;
        default:                StateMachine_SetState(STATE_STRAIGHT); break;
    }
}

// ====================================================================
// 1. 直行/循迹状态
// ====================================================================
void State_HandleStraight(void) {
    // 1. 检查环岛 (最高优先级)
    if (TrackDetect_CheckRingEntry()) {
        StateMachine_SetState(STATE_RING);
        return;
    }

    /* 检查直角弯 */
    // 调用刚才重写的 CheckSharpTurn
    if (TrackDetect_CheckSharpTurn()) {
        Buzzer_Beep(50); // 短鸣提示检测到直角
        StateMachine_SetState(STATE_SHARP_TURN);
        return;
    }

    // 3. 新增：中间电感 4095 判定
    if (g_trackSensor.filtered[SENSOR_M] >= 4095) {
        Buzzer_Beep(50); // 蜂鸣器短响一声提示触发
        StateMachine_SetState(STATE_RIGHT_BLIND); // 跳转到盲跑状态
        return;
    }

    // 4. 正常循迹计算
    Track_Line_Control();

    // 4. 避障触发检测 (极速版)
    float dist = Ultrasonic_GetDistance();

    // 过滤无效数据，检测小于 50cm
    if (dist > 0.1f && dist < OBS_DETECT_DIST) {
        s_distCheckCount++;
    } else {
        s_distCheckCount = 0; // 一旦大于50cm，计数器立马清零，防止累计
    }

    // 连续 2 次检测到就触发 (2 * 20ms = 40ms 反应时间)
    if (s_distCheckCount >= 2) {
        s_distCheckCount = 0;

        // 切换到避障状态
        StateMachine_SetState(STATE_OBSTACLE);
    }
}

// ====================================================================
// 2. 环岛状态 (强制右转版)
// ====================================================================
void State_HandleRing(void) {
    float currentYaw = g_mpu6050.yaw;
    int16_t rawDev = TrackDetect_GetDeviation();
    bool isCenterStrong = (g_trackSensor.centerValue > 3295);
    bool isSideBalanced = (abs(rawDev) < 300);
    uint32_t phaseTime = GetPhaseDuration();
    float anglePassed = 0.0f;

    switch (g_stateMachine.ringPhase) {
        case RING_DETECTED:
            Buzzer_Beep(100); // 蜂鸣器短鸣

            g_stateMachine.ringEntryYaw = currentYaw;

            // 强制右环
            g_stateMachine.ringDirection = -1;

            g_stateMachine.ringTargetYaw = currentYaw + (g_stateMachine.ringDirection * RING_ENTRY_ANGLE);
            g_stateMachine.ringPhase = RING_ENTRY_TURN;
            g_stateMachine.obsPhaseTime = GetTick();
            break;

        case RING_ENTRY_TURN:
            Yaw_control_200hz((int)g_stateMachine.ringTargetYaw);
            // 删掉v_target赋值，让Yaw_control_200hz内部的控制量生效

            if (phaseTime > 200 && isCenterStrong && isSideBalanced) {
                g_stateMachine.ringPhase = RING_CIRCLING;
                g_stateMachine.obsPhaseTime = GetTick();
                return;
            }
            if (fabs(currentYaw - g_stateMachine.ringTargetYaw) < 5.0f || phaseTime > 1000) {
                g_stateMachine.ringPhase = RING_CIRCLING;
                g_stateMachine.obsPhaseTime = GetTick();
            }
            break;

        case RING_CIRCLING:
            Track_Line_Control();
            anglePassed = fabs(currentYaw - g_stateMachine.ringEntryYaw);
            if (anglePassed >= 360.0f) {
                g_stateMachine.ringTargetYaw = currentYaw;
                g_stateMachine.ringPhase = RING_EXIT_TURN;
                g_stateMachine.obsPhaseTime = GetTick();
            }
            break;

        case RING_EXIT_TURN:
            Yaw_control_200hz((int)g_stateMachine.ringTargetYaw);
            // 删掉v_target赋值，让Yaw_control_200hz内部的控制量生效

            // 计时结束，出环
            if (phaseTime >= 600) {
                StateMachine_SetState(STATE_STRAIGHT);
            }
            break;

        default:
            StateMachine_SetState(STATE_STRAIGHT);
            break;
    }
}

// ====================================================================
/* 避障动作执行：完成 3 次后停车 */
// ====================================================================
void State_HandleObstacle(void) {
    float currentYaw = g_mpu6050.yaw;
    float speed = (float)setup_speed_l; // 全程保持正常速度

    switch (g_stateMachine.obsPhase) {
        // ------------------------------------------------
        // 阶段 A: 初始化与右转预备
        // ------------------------------------------------
        case OBS_DETECTED:
            Buzzer_Beep(100); // 蜂鸣器短鸣提示触发

            // 设定目标：当前向右偏 60 度
            g_stateMachine.obsTargetYaw = currentYaw - 60.0f;

            // 进入右偏行驶阶段
            g_stateMachine.obsPhase = OBS_LEFT_TURN;
            g_stateMachine.obsPhaseTime = GetTick();
            break;

        // ------------------------------------------------
        // 阶段 B: 右偏60度并行驶 1s
        // ------------------------------------------------
        case OBS_LEFT_TURN:
            Yaw_control_200hz((int)g_stateMachine.obsTargetYaw);
            // 删掉v_target赋值，让Yaw_control_200hz内部的控制量生效

            // 计时 1000ms
            if (GetPhaseDuration() >= 1000) {
                // 时间到，准备回正抢线
                // 设定目标：向左偏 120 度 (即相对于最开始的直道左偏60度切回)
                g_stateMachine.obsTargetYaw = g_stateMachine.obsTargetYaw + 120.0f;

                g_stateMachine.obsPhase = OBS_RIGHT_TURN;
                g_stateMachine.obsPhaseTime = GetTick();
            }
            break;

        // ------------------------------------------------
        // 阶段 C: 左偏120度 + 抢线
        // ------------------------------------------------
        case OBS_RIGHT_TURN:
            Yaw_control_200hz((int)g_stateMachine.obsTargetYaw);
            // 删掉v_target赋值，让Yaw_control_200hz内部的控制量生效

            // 延时 300ms 后开始看电感
            if (GetPhaseDuration() > 300) {
                int16_t dev = TrackDetect_GetDeviation();
                // 抓到线了 (偏差变小)
                if (abs(dev) < 400) {
                    /* 避障动作完成 */
                    s_avoidPassCount++; // 计数器+1

                    if (s_avoidPassCount >= 3) {
                        // 已经是第3次避障完成了 -> 停车！
                        Buzzer_Beep(500); // 长鸣提示结束
                        ModeControl_SetMode(MODE_STOP);
                        MotorControl_SetSpeed(0, 0);
                    } else {
                        // 还没到3次 -> 继续跑直道
                        StateMachine_SetState(STATE_STRAIGHT);
                    }
                }
            }

            // 超时强制切回 (防止一直找不到线)
            if (GetPhaseDuration() > OBS_PHASE_TIMEOUT_MS) {
                 s_avoidPassCount++; // 即使是超时切回也算一次
                 if (s_avoidPassCount >= 3) {
                     ModeControl_SetMode(MODE_STOP);
                     MotorControl_SetSpeed(0, 0);
                 } else {
                     StateMachine_SetState(STATE_STRAIGHT);
                 }
            }
            break;

        // 废弃状态占位
        case OBS_GO_STRAIGHT:
            g_stateMachine.obsPhase = OBS_RIGHT_TURN;
            break;

        default:
            StateMachine_SetState(STATE_STRAIGHT);
            break;
    }
}

// ====================================================================
// 4. 急转弯处理 (保持不变)
// ====================================================================
void State_HandleSharpTurn(void) {
    Yaw_control_200hz((int)(g_stateMachine.sharpTargetYaw));

    // 提高直角弯盲跑动力
    v_target_l = 150;
    v_target_r = 150;

    uint32_t phase_time = GetPhaseDuration();

    // 强制抢线：盲跑400ms后，如果中心电感检测到黑线，立刻切换回循迹
    if (phase_time > 400) {
        // 中心电感值检测（使用归一化后的值）
        if (g_trackSensor.centerValue > 1500) {
            StateMachine_SetState(STATE_STRAIGHT);
            return;
        }
    }

    // 最大盲跑时间800ms，防止长时间失控
    if (phase_time > 800) {
        StateMachine_SetState(STATE_STRAIGHT);
    }
}

// ====================================================================
// 5. 右盲跑状态处理
// ====================================================================
void State_HandleRightBlind(void) {
    uint32_t duration = GetPhaseDuration(); // 获取进入该状态后的持续时间

    if (duration < 800) { // 0.5s = 500ms
        // 执行向右旋转动作
        // 左轮快，右轮反转实现右转
        v_target_l = 250; // 左轮快
        v_target_r = 0; // 右轮反转
    }
    else {
        // 时间到，切回正常循迹
        StateMachine_SetState(STATE_STRAIGHT);
    }
}

// 状态切换函数
void StateMachine_SetState(VehicleState_e newState) {
    if (g_stateMachine.currentState == newState) return;

    g_stateMachine.lastState = g_stateMachine.currentState;
    g_stateMachine.currentState = newState;
    g_stateMachine.stateEnterTime = GetTick();
    g_stateMachine.obsPhaseTime = GetTick();

    switch (newState) {
        case STATE_RING:
            g_stateMachine.ringPhase = RING_DETECTED;
            break;
        case STATE_OBSTACLE:
            g_stateMachine.obsPhase = OBS_DETECTED;
            break;
        case STATE_SHARP_TURN:
            g_stateMachine.sharpPhase = SHARP_DECEL;
            if (TrackDetect_GetDeviation() > 0) {
                g_stateMachine.sharpTargetYaw = g_mpu6050.yaw - 70.0f;
            } else {
                g_stateMachine.sharpTargetYaw = g_mpu6050.yaw + 70.0f;
            }
            break;
        default:
            break;
    }
}