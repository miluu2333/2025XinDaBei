/* 
  * mt_ultrasonic.c - 故障诊断版 
  * 作用：通过输出特定的"错误距离"，告诉你程序死在哪一步 
  */ 
 #include "mt_ultrasonic.h" 
 #include "main.h" 
 #include <string.h> 
 #include "Delay.h" 
 
 volatile Ultrasonic_t g_ultrasonic; 
 
 void Ultrasonic_Init(void) {
    /* g_ultrasonic 带 volatile，直接 memset 会丢掉限定符语义；
       改用静态零值整体赋值，效果相同且保留易失性访问。 */
    static const Ultrasonic_t s_ultrasonicZero = {0};
    g_ultrasonic = s_ultrasonicZero; 
    
    // 1. 强制初始化 GPIOB
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); 
    
    GPIO_InitTypeDef GPIO_InitStructure; 
    
    // Trig -> PB0 (推挽输出)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
    
    // Echo -> PB1 (浮空输入)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure); 
    
    // 默认拉低 Trig
    GPIO_ResetBits(GPIOB, GPIO_Pin_0); 
    
    /* 采样间隔 20 ms，约 50 Hz 刷新。 */
    g_ultrasonic.measureInterval = 20;
    g_ultrasonic.enable = true;
} 
 
 // 诊断核心函数 
 void Ultrasonic_Polling_Measure(void) { 
     // -------------------------------------------------- 
     // 阶段1：检查定时器是否存活 
     // -------------------------------------------------- 
     uint16_t t_check1 = TIM_GetCounter(TIM3); 
     Delay_us(10); 
     uint16_t t_check2 = TIM_GetCounter(TIM3); 
     
     /* 诊断码 777：TIM3 未计数（时钟或配置异常） */
     if (t_check1 == t_check2) { 
         g_ultrasonic.distance = 777.0f; 
         return; 
     } 
 
     // -------------------------------------------------- 
     // 阶段2：发射信号 
     // -------------------------------------------------- 
     GPIO_SetBits(GPIOB, GPIO_Pin_0);   // Trig 高 
     Delay_us(20);                      // 维持 20us 
     GPIO_ResetBits(GPIOB, GPIO_Pin_0); // Trig 低 
     
     // --------------------------------------------------
     // 阶段3：等待回波 (Echo) 变高
     // --------------------------------------------------
     // 加上超时保护，防止死循环
     uint32_t timeout = 0;
     while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0) {
         timeout++;
         Delay_us(1);
         if (timeout > 2000) { // 等待2ms没反应就拉倒
             g_ultrasonic.distance = 888.0f;
             return;
         }
     } 
     
     // 记录高电平开始时间 
     uint16_t t1 = TIM_GetCounter(TIM3); 
     
     // --------------------------------------------------
     // 阶段4：等待回波 (Echo) 变低 (测量核心优化)
     // --------------------------------------------------
     timeout = 0;
     while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 1) {
         timeout++;
         Delay_us(1);
         
         /* 超时截断 */
         // 只要超过 6000us (约 1米)，就认为前方空旷，直接退出。
         // 之前是 30000us，会导致主循环卡顿严重。
         if (timeout > 6000) {
             g_ultrasonic.distance = 150.0f;  /* 超时按安全距离处理 */
             return;
         }
     } 
     
     // 记录结束时间 
     uint16_t t2 = TIM_GetCounter(TIM3); 
     
     // -------------------------------------------------- 
     // 阶段5：计算结果 
     // -------------------------------------------------- 
     uint16_t dt_us = t2 - t1; // 自动处理溢出 
     
     float rawDistance = (float)dt_us / 58.0f; 
     
     // 正常更新 
     if (rawDistance > 0.1f && rawDistance < 500.0f) { 
         g_ultrasonic.distance = rawDistance; 
     } 
 } 
 
 void Ultrasonic_Update(void) { 
     if (!g_ultrasonic.enable) return; 
     
     static uint32_t lastTime = 0; 
     uint32_t now = GetTick(); 
     
     if (now - lastTime >= g_ultrasonic.measureInterval) { 
         Ultrasonic_Polling_Measure(); 
         lastTime = now; 
     } 
 } 
 
 // 占位符 
 void Ultrasonic_TriggerMeasurement(void) {} 
 void Ultrasonic_EchoCallback(void) {} 
 float Ultrasonic_GetDistance(void) { return g_ultrasonic.distance; } 
 // ... (其他函数保持原样) 
 /* 障碍物判定开关：0 保持比赛现场的占位行为，1 启用基于距离的判断。
   启用后会影响避障状态机，改前须先在实车上验证阈值。 */
#define ULTRASONIC_OBSTACLE_CHECK 0
#define ULTRASONIC_OBSTACLE_CM    30.0f

bool Ultrasonic_IsObstacleDetected(void) {
#if ULTRASONIC_OBSTACLE_CHECK
    return (g_ultrasonic.distance > 0.1f) &&
           (g_ultrasonic.distance < ULTRASONIC_OBSTACLE_CM);
#else
    /* 占位实现：现场调试期间障碍物判定由状态机内部完成，此接口恒返回否。 */
    return false;
#endif
} 
 void Ultrasonic_Enable(bool enable) { g_ultrasonic.enable = enable; } 
 void Ultrasonic_SetObstacleThreshold(float threshold) {}