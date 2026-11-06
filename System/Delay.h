#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

// 声明全局变量
extern volatile uint32_t g_tick_ms;

// 阻塞式延时函数（保留，用于初始化阶段）
void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);
void Delay_s(uint32_t s);

// 非阻塞式延时函数（SysTick实现，推荐用于主循环）
void SysTick_Init(void);
uint32_t Get_SysTick_ms(void);
uint8_t Delay_ms_NonBlock(uint32_t start_tick, uint32_t delay_ms);

#endif
