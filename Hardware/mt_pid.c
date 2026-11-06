#include "main.h"
#include "mt_pid.h"
#include "mt_mpu6050.h"
#include "mt_motor.h"
#include "mt_track_detect.h"
#include <math.h>   
#include <stdlib.h> 

extern int setup_speed_l;

MotorController_t g_motorController;
volatile SteeringPID_Config_t g_pidConfig = {0};
float v_target_l = 0.0f;
float v_target_r = 0.0f;

void MotorControl_Init(void) {
    Motor_Init();
    

    /* 单环低速档参数 */

    
    // 1. 角度锁参数 (Yaw -> PWM)
    g_pidConfig.angle_kp = 65.0f;     // 角度环比例 (用于环岛打角)
    g_pidConfig.angle_kd = 0.5f;       // 角度环微分
    
    // 2. 动力转移分配相关参数
    g_pidConfig.outer_kp = 280.0f;      // 外环P，提高响应速度
    g_pidConfig.outer_kd = 50.5f;      // 外环D
    g_pidConfig.feedforward = 300.0f; // 暴力前馈，建议设为>300用于直角弯
}

// 辅助限幅宏
/* 限幅：用 inline 函数替代宏，避免参数被重复求值。 */
static inline float ClampF(float x, float lo, float hi) {
    return (x > hi) ? hi : ((x < lo) ? lo : x);
}

/* 单轮 PWM 上限，对应 TIM1 ARR = 999 的占空比量程。 */
#define ABS_MAX_PWM   200.0f
#define YAW_MAX_OUT     400.0f
#define MOTOR_DEAD_ZONE 0.0f 
// ============================================================
// 模式 1: 纯巡线控制 (无陀螺仪参与，纯 Error 前馈)
// ============================================================
void Track_Line_Control(void) {
    // A. 偏差计算
    float raw_error = (float)TrackDetect_GetDeviation();
    float error_norm = ClampF(raw_error / 1000.0f, -1.0f, 1.0f);
    
    // --- 核心修改：非线性放大 --- 
    // 使用 powf(abs) 保持符号，这会显著拉高 0 附近的增益
    float error_abs = fabs(error_norm);
    if (error_abs > 0.001f) {
        // 放大后的误差映射，0.7次方可以让小偏差瞬间获得较大动力
        error_norm = (error_norm / error_abs) * powf(error_abs, 0.8f);
    }
    // -------------------------

    static float last_error_norm = 0;
    float error_rate = error_norm - last_error_norm;
    last_error_norm = error_norm;

    // B. 基础转身量计算
    float turn_offset = (error_norm * g_pidConfig.outer_kp) + 
                        (error_rate * g_pidConfig.outer_kd) + 
                        (error_norm * g_pidConfig.feedforward);

    float base = (float)setup_speed_l;
    float out_l, out_r;

    // C. 完全对称化处理
    // 左右轮使用相同的系数，避免一侧灵敏一侧迟钝
    out_l = base - turn_offset;
    out_r = base + turn_offset;

    // D. 移除动力转移逻辑，避免阶跃式跳变导致的漂移
    // 动力转移逻辑会在偏差跨越零点时产生非线性跳变，破坏S弯的平滑性

    // E. 限幅输出
    /* 注意：左右轮限幅区间目前不对称——左轮钳位在 [0, 200]，
       右轮为 [-200, 380]。这是调试期留下的状态，详见 KNOWN_ISSUES.md。 */
    v_target_l = ClampF(out_l, 0.0f, ABS_MAX_PWM);
    v_target_r = ClampF(out_r, -ABS_MAX_PWM, 1.9f * ABS_MAX_PWM);
}

// ============================================================
// 模式 2: 角度环控制 (用于特定时间打角)
// ============================================================
void Yaw_control_200hz(int target_angle) {
    // 获取当前角度
    float current_yaw = g_mpu6050.yaw;
    
    // 计算角度误差 (目标 - 当前)
    float angle_error = (float)target_angle - current_yaw;
    
    // 计算微分
    static float last_angle_error = 0;
    float angle_rate = angle_error - last_angle_error;
    last_angle_error = angle_error;
    
    // PD 计算输出
    float angle_out = (angle_error * g_pidConfig.angle_kp) + (angle_rate * g_pidConfig.angle_kd);
    
    // 限制修正力度
    angle_out = ClampF(angle_out, -YAW_MAX_OUT, YAW_MAX_OUT);
    
    // 叠加到基础速度上
    // 如果 angle_out > 0 (需要向左转)，则左轮减，右轮加 (假设左转对应 yaw 增加)
    // 注意：这里方向要根据你的陀螺仪方向实测调整
    // 假设：Yaw逆时针增加(左转为正)，为了追赶目标(大值)，需要左转 -> 左轮减，右轮加
    
    // 修正：通常 target > current 时，我们需要往正方向转(左转)
    // 左转 = 左轮慢，右轮快
    v_target_l = (float)setup_speed_l - angle_out;
    v_target_r = (float)setup_speed_l + angle_out;
    
    /* 电机死区补偿：当前设为 0，即不补偿。
       若更换驱动或电机出现低速不转，把 MOTOR_DEAD_ZONE 调到实测阈值即可。 */
    if (v_target_l >  1.0f) v_target_l += MOTOR_DEAD_ZONE;
    if (v_target_l < -1.0f) v_target_l -= MOTOR_DEAD_ZONE;
    if (v_target_r >  1.0f) v_target_r += MOTOR_DEAD_ZONE;
    if (v_target_r < -1.0f) v_target_r -= MOTOR_DEAD_ZONE;
}

void MotorControl_Update(void) {
    Motor_Turn((int16_t)v_target_l, (int16_t)v_target_r);
}

void MotorControl_SetSpeed(float left, float right) {
    v_target_l = left; v_target_r = right;
}
