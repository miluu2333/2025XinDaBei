# 已知状态与维护提示

本仓库是比赛完赛版本，代码里保留了大量现场调试的临时状态。整理归档时**没有改变实际控制行为**，只修复了明确的编码缺陷（除零、空操作、宏重复求值、越界保护）、消除了缓冲区与初始化隐患，并统一注释。

以下是恢复或二次开发前必须确认的事项，按影响程度排序。

---

## 1. 速度闭环实际是开环（影响最大）

**现象**

- `User/main.c` 中 `Encoder_Init()` 与 `Encoder_Update()` 被注释
- 主循环里 `spd_L` / `spd_R` 固定为 `0.0f`
- `g_motorController.currentSpeed` 因此恒为 0

**原因**：TIM3 被让给其他外设，编码器未接入。

**影响**：`mt_pid.c` 中的速度相关反馈不参与运算，实际控制为开环 PWM 输出。

**恢复步骤**

1. 确认硬件上编码器已接到 TIM3 / TIM4 对应引脚
2. 取消 `Encoder_Init()` 与 `Encoder_Update()` 的注释
3. 把 `spd_L` / `spd_R` 改为 `Encoder_GetSpeed(...)`
4. 重新整定 PID 参数

---

## 2. 控制周期与文档不一致

**现象**：`User/main.c` 主循环 `CONTROL_PERIOD = 2`（2 ms），而 `技术报告.md` 写的是 10 ms / 100 Hz。

**影响**：以代码为准是 500 Hz。若按报告里的 10 ms 去整定微分项，实际增益会差 5 倍。

**建议**：确认实际运行周期后统一文档与代码，微分系数按真实周期重算。

---

## 3. 左右轮限幅不对称

**现象**：`mt_pid.c` 的 `Track_Line_Control()` 中

```c
v_target_l = ClampF(out_l, 0.0f, ABS_MAX_PWM);                  // [0, 200]
v_target_r = ClampF(out_r, -ABS_MAX_PWM, 1.9f * ABS_MAX_PWM);  // [-200, 380]
```

原注释写"允许左右轮反转、恢复对称性"，但左轮下限为 0，实际**不能反转**；右轮上限 380 也与"对称"不符。

**影响**：大偏差时左右轮动力不对称，S 弯可能单向漂移。

**建议**：这是调试残留，改为两侧均 `[-ABS_MAX_PWM, ABS_MAX_PWM]` 前应实测确认。归档版本保留原数值，不做行为变更。

---

## 4. 基础速度左右不一致

`BASE_SPEED_LEFT = 90`、`BASE_SPEED_RIGHT = 100`，但 `mt_pid.c` 里 `Track_Line_Control()` 与 `Yaw_control_200hz()` 都只用 `setup_speed_l` 作为双侧基准，`setup_speed_r` 从未被使用。

**影响**：左右轮静态差速被忽略，直行时可能需要靠 PID 持续补偿。

---

## 5. 死区补偿当前无效

`MOTOR_DEAD_ZONE` 定义为 `0.0f`，补偿分支等价于空操作。原实现里是 `float dead_zone = 0.0f;` 后接四个 `+= 0`，现已提成命名常量，行为不变。

**建议**：更换驱动或电机后若出现低速不转，把该值调到实测启动阈值。

---

## 6. 调试残留代码

| 位置 | 内容 | 说明 |
| --- | --- | --- |
| `User/main.c` 主循环 | 读 PB6 / PB7 电平镜像到 PC13 LED | 现场查线用，可删 |
| `User/main.c` 初始化 | `Force_Fix_Conflict()` | 统一修正引脚复用冲突，不要随意删除 |
| `User/main.c` 初始化 | PB6 / PB7 上拉输入重配 | 与各模块初始化顺序相关 |
| `Hardware/mt_motor.c` | `RAllPulse` 积分 | 编码器禁用期间该值不增长 |

---

## 7. GPIO 引脚复用

`GPIO_Pin_Summary.md` 中 PA0 同时登记为"左电机控制 A 相"与"ADC 赛道传感器"，PA1–PA5 也有类似情况。

**接线时以 `GPIO_Pin_Summary.md` 为准**，不要按各模块单独文档拼接，否则可能短路。

---

## 8. 本次整理做了什么

| 文件 | 改动 | 行为影响 |
| --- | --- | --- |
| `User/main.c` | `Buzzer_Tone()` 增加 `frequency == 0` 与 `period == 0` 保护 | 无（仅防崩溃） |
| `User/main.c` | 基础速度提成 `BASE_SPEED_LEFT/RIGHT` 宏 | 无 |
| `User/main.c` | 清理 `【修改】/【强制修复】` 等临时注释，改为规范说明 | 无 |
| `Hardware/mt_pid.c` | `LIMIT` 宏改为 `static inline ClampF()` | 无（消除参数重复求值风险） |
| `Hardware/mt_pid.c` | 死区 `+= 0` 空操作改为命名常量 | 无 |
| `Hardware/mt_pid.c` | 限幅注释修正为与实际代码一致 | 无 |
| `Hardware/mt_motor.c` | `Motor_Turn()` 增加 `PWM_MAX_DUTY` 越界保护 | 正常范围内无影响 |
| `Hardware/mt_motor.c` | 删除注释掉的 `ClampPWM` 死代码、统一缩进 | 无 |

**未改动**：所有 PID 参数、控制律、状态机逻辑、引脚分配、中断配置，以及 `Library/`、`Start/` 下的官方库文件。

---

## 9. 超声波模块的三处现场状态

| 位置 | 现象 | 影响 |
| --- | --- | --- |
| `Ultrasonic_IsObstacleDetected()` | 原为恒返回 `false` 的占位函数 | 该接口从未真正参与避障判定 |
| `Ultrasonic_Polling_Measure()` | 用 `777.0f` / `888.0f` / `150.0f` 表示诊断状态 | 这些值会写进 `distance`，调用方若当真实距离使用会误判 |
| `g_ultrasonic` | 声明为 `volatile`，原用 `memset` 清零 | 丢失易失性限定语义，编译器优化下行为不可靠 |

**已处理但保持行为**：障碍物判定改为编译期开关 `ULTRASONIC_OBSTACLE_CHECK`（默认 `0`，仍是占位行为），启用后才按 `ULTRASONIC_OBSTACLE_CM` 判断；清零改为静态零值整体赋值。**启用开关会改变避障状态机行为，必须先在实车上验证阈值。**

诊断码建议后续改为独立的状态枚举，不要复用 `distance` 字段。

---

## 10. 迭代 6 改动清单

| 文件 | 改动 | 行为影响 |
| --- | --- | --- |
| `Hardware/mt_vofa.c` | `sprintf` → `snprintf`，按 `sizeof(txBuffer)` 限制并钳制帧长 | 正常范围内无影响，消除缓冲区溢出 |
| `Hardware/mt_mpu6050.c` | WHO_AM_I 校验改为 `MPU6050_STRICT_ID_CHECK` 开关（默认 0） | 无，默认仍跳过校验，但保留诊断值 |
| `Hardware/mt_mpu6050.c` | 新增 `g_mpu6050DeviceId` 保存读回的器件 ID | 无，供在线诊断 |
| `Hardware/mt_encoder.c` | `PI` → `ENCODER_PI`，避免与 `math.h` 宏冲突 | 无 |
| `Hardware/mt_encoder.c` | `GPIO_InitStructure.GPIO_Speed` 显式初始化 | 输入模式下无影响，消除结构体残留值 |
| `Hardware/mt_track_detect.c` | 能量累加循环 `5` → `SENSOR_MAX` | 无（当前枚举值即 5），消除数量变更时的越界隐患 |
| `Hardware/mt_ultrasonic.c` | `volatile` 结构体改用零值整体赋值，移除 `string.h` | 无 |
| 全仓 `.c/.h` | 30 处 `【修改】/【关键修复】` 类临时标记改为正式注释 | 无 |

所有改动均通过花括号、圆括号平衡自检；控制律与参数未动。
