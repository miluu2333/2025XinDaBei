#include "Delay.h"
#include "main.h"  // 包含 main.h 以获取 g_systemTick 的声明

// 引用 main.c 中定义的全局时间变量
extern volatile uint32_t g_systemTick;

/**
  * @brief  初始化 SysTick
  */
void SysTick_Init(void)
{
    // 配置 SysTick 为 1ms 中断
    // SystemCoreClock 通常为 72000000
    if (SysTick_Config(SystemCoreClock / 1000))
    {
        // 初始化失败，卡死
        while (1);
    }
    // 关闭 SysTick 默认优先级，改为最低，防止阻塞其他中断
    NVIC_SetPriority(SysTick_IRQn, 0xFF);
}

/**
  * @brief  获取系统运行时间（毫秒）
  */
uint32_t Get_SysTick_ms(void)
{
    return g_systemTick;
}

/**
  * @brief  非阻塞式延时检查
  * @param  start_tick: 起始时间戳
  * @param  delay_ms: 延时时长
  * @retval 1: 时间已到, 0: 时间未到
  */
uint8_t Delay_ms_NonBlock(uint32_t start_tick, uint32_t delay_ms)
{
    // 使用减法计算差值，即使溢出也能正确判断
    if ((g_systemTick - start_tick) >= delay_ms)
    {
        return 1;
    }
    return 0;
}

/**
  * @brief  毫秒级阻塞延时
  */
void Delay_ms(uint32_t ms)
{
    uint32_t start = g_systemTick;
    while ((g_systemTick - start) < ms);
}

/**
  * @brief  微秒级阻塞延时 (粗略估计，适用于 72MHz)
  */
void Delay_us(uint32_t us)
{
    // 72MHz 下，指令周期约 1/72 us。
    // 简单的循环计数，需要根据编译器优化调整。
    // 这里给一个经验值，大约 1us 循环 9-10 次左右
    uint32_t i = us * 9; 
    while(i--);
}
