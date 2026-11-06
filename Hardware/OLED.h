#ifndef OLED_H
#define OLED_H

#include "stm32f10x.h"

/*引脚配置*/
#define OLED_W_SCL(x)    GPIO_WriteBit(GPIOB, GPIO_Pin_10, (BitAction)(x))
#define OLED_W_SDA(x)    GPIO_WriteBit(GPIOB, GPIO_Pin_11, (BitAction)(x))

// 函数声明
void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowTelemetryData(void);
void OLED_I2C_Start(void);
void OLED_I2C_Stop(void);
void OLED_I2C_SendByte(uint8_t Byte);
void OLED_WriteCommand(uint8_t Command);
void OLED_WriteData(uint8_t Data);
void OLED_SetCursor(uint8_t Y, uint8_t X);

#endif


