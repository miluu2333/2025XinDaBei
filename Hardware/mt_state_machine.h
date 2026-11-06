#ifndef MT_STATE_MACHINE_H_
#define MT_STATE_MACHINE_H_

#include "main.h"

// 车辆主状态
typedef enum {
    STATE_STRAIGHT = 0,   // 直道/常规循迹
    STATE_RING,           // 环岛处理
    STATE_SHARP_TURN,     // 直角弯处理
    STATE_OBSTACLE,       // 障碍物处理
    STATE_RIGHT_BLIND     // 右盲跑处理
} VehicleState_e;

// 环岛子状态 (Ring Phases)
typedef enum {
    RING_DETECTED = 0,    // 0. 刚检测到环岛 (减速确认)
    RING_ENTRY_TURN,      // 1. 入环转向 (陀螺仪盲转)
    RING_CIRCLING,        // 2. 环内巡线 (正常PID)
    RING_EXIT_TURN,       // 3. 出环转向 (陀螺仪盲转)
    RING_RECOVER          // 4. 出环回正 (找线)
} RingPhase_e;

// 障碍物子状态 (Obstacle Phases)
typedef enum {
    OBS_DETECTED = 0,     // 0. 检测到 (刹车)
    OBS_LEFT_TURN,        // 1. 向左变道 (陀螺仪)
    OBS_GO_STRAIGHT,      // 2. 直行通过 (编码器/时间)
    OBS_RIGHT_TURN,       // 3. 向右回正 (陀螺仪)
    OBS_RECOVER           // 4. 寻找赛道 (电感判定)
} ObstaclePhase_e;

// 直角弯子状态
typedef enum {
    SHARP_DECEL = 0,      // 减速
    SHARP_TURN,           // 转向
    SHARP_RECOVER         // 恢复
} SharpTurnPhase_e;

// 状态机结构体
typedef struct {
    VehicleState_e currentState;
    VehicleState_e lastState;
    uint32_t stateEnterTime;      // 进入状态的时间戳
    
    // 环岛相关
    RingPhase_e ringPhase;
    float ringTargetYaw;          // 目标角度
    
    /* 环岛积分锁计数变量 */
    float ringEntryYaw;           // 入环时的基准角度 (0°参考点)
    int8_t ringDirection;         // 环岛方向: 1为左环(Yaw增加), -1为右环(Yaw减小)
    
    // 障碍物相关
    ObstaclePhase_e obsPhase;
    float obsTargetYaw;           // 目标角度
    uint32_t obsPhaseTime;        // 阶段计时
    
    // 直角弯相关
    SharpTurnPhase_e sharpPhase;
    float sharpTargetYaw;
    
    // 标志位
    bool lockStatus;              // 状态锁定 (防止跳变)
} StateMachine_t;

extern StateMachine_t g_stateMachine;

// 函数声明
void StateMachine_Init(void);
void StateMachine_Update(void);
void StateMachine_SetState(VehicleState_e newState);
VehicleState_e StateMachine_GetCurrentState(void);

// 各状态处理函数
void State_HandleStraight(void);
void State_HandleRing(void);
void State_HandleObstacle(void);
void State_HandleSharpTurn(void);
void State_HandleRightBlind(void);

#endif /* MT_STATE_MACHINE_H_ */

