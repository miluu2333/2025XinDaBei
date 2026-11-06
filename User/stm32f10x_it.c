/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "main.h" // ?? main.h ????????
#include "Delay.h"
#include "mt_ultrasonic.h" // 添加超声波模块头文件，确保Ultrasonic_EchoCallback函数可用

// === ???????? ===
// g_systemTick ? main.h ???? extern volatile,? main.c ???
extern volatile uint32_t g_systemTick;

/** @addtogroup STM32F10x_StdPeriph_Template
  * @{
  */

/******************************************************************************/
/* Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

void NMI_Handler(void) { }
void HardFault_Handler(void) { while (1) { } }
void MemManage_Handler(void) { while (1) { } }
void BusFault_Handler(void) { while (1) { } }
void UsageFault_Handler(void) { while (1) { } }
void SVC_Handler(void) { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void) { }

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
    // === ????:???????? ===
    // ?? g_systemTick ?? SysTick ??????? (? 1ms)
    g_systemTick++;
    
    // 更新Delay.c中的全局计时器
//    g_tick_ms++;
}

/******************************************************************************/
/* STM32F10x Peripherals Interrupt Handlers                   */
/******************************************************************************/

/* ... (其他中断服务函数) ... */

/**
  * @brief  This function handles EXTI Line1 interrupt request for ultrasonic ECHO pin.
  * @param  None
  * @retval None
  */
void EXTI1_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line1) != RESET)
    {
        // 调用超声波回波回调函数
        Ultrasonic_EchoCallback();
        
        // 清除中断标志位
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
}

/**
  * @}
  */ 

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
