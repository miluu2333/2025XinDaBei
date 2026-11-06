#include "stm32f10x.h"
#include "OLED_Font.h"
#include "mt_vofa.h" // 添加遥测数据头文件
#include "Delay.h"   // 添加延时函数头文件
#include "OLED.h"

/*引脚配置*/
#define OLED_W_SCL(x)        GPIO_WriteBit(GPIOB, GPIO_Pin_10, (BitAction)(x))
#define OLED_W_SDA(x)        GPIO_WriteBit(GPIOB, GPIO_Pin_11, (BitAction)(x))

/*引脚初始化*/
void OLED_I2C_Init(void)
{
    // 启用GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // 初始化I2C引脚
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; // SCL
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11; // SDA
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    
    // 确保初始状态为高电平
    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

// 前向声明
void OLED_I2C_Start(void);
void OLED_I2C_Stop(void);

/**
  * @brief  优化后的软件初始化流程
  *         针对没有硬件复位引脚的情况
  */
void OLED_SoftwareInit(void)
{
    // 1. 初始化I2C接口
    OLED_I2C_Init();
    
    // 2. 发送多个起始条件以确保模块唤醒
    for(int i = 0; i < 3; i++) {
        OLED_I2C_Start();
        Delay_us(50);
        OLED_I2C_Stop();
        Delay_ms(10);  // 模块响应时间
    }
}



/**
  * @brief  I2C开始（优化时序版）
  * @param  无
  * @retval 无
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	Delay_us(1);
	OLED_W_SCL(1);
	Delay_us(1);
	OLED_W_SDA(0);
	Delay_us(1);
	OLED_W_SCL(0);
	Delay_us(1);
}

/**
  * @brief  I2C停止（优化时序版）
  * @param  无
  * @retval 无
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	Delay_us(1);
	OLED_W_SCL(1);
	Delay_us(1);
	OLED_W_SDA(1);
	Delay_us(1);
}

/**
  * @brief  I2C发送一个字节（优化时序版）
  * @param  Byte 要发送的一个字节
  * @retval 无
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(!!(Byte & (0x80 >> i)));
		Delay_us(1); // 时序优化：确保SDA信号稳定后再翻转时钟
		OLED_W_SCL(1);
		Delay_us(1); // 时序优化：增加时钟高电平持续时间
		OLED_W_SCL(0);
		Delay_us(1); // 时序优化：增加时钟低电平持续时间
	}
	OLED_W_SCL(1);	//额外的一个时钟，不处理应答信号
	Delay_us(1);
	OLED_W_SCL(0);
	Delay_us(1);
}

/**
  * @brief  OLED写命令
  * @param  Command 要写入的命令
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x00);		//写命令
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

/**
  * @brief  OLED写数据
  * @param  Data 要写入的数据
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x40);		//写数据
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

/**
  * @brief  OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~7
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					//设置Y位置
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位
}

/**
  * @brief  OLED清屏
  * @param  无
  * @retval 无
  */
void OLED_Clear(void)
{  
	uint8_t i, j;
	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);
		for(i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
}

/**
  * @brief  OLED显示一个字符
  * @param  Line 行位置，范围：1~4
  * @param  Column 列位置，范围：1~16
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//设置光标位置在上半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);			//显示上半部分内容
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//设置光标位置在下半部分
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);		//显示下半部分内容
	}
}

/**
  * @brief  OLED显示字符串
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  String 要显示的字符串，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
  * @brief  OLED显示数字（十进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

/**
  * @brief  OLED显示数字（二进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  Length 要显示数字的长度，范围：1~16
  * @retval 无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
    // 1. 软件唤醒流程
    OLED_SoftwareInit();
    
    // 2. 基础初始化
    Delay_ms(500);  // 确保上电稳定
    
    // 3. 发送关键初始化命令序列（简化版）
    const uint8_t init_sequence[] = {
        0xAE, // 关闭显示
        0x20, // 设置内存寻址模式
        0x10, // 00=水平寻址 01=垂直寻址 10=页面寻址
        0xB0, // 设置页起始地址
        0xC8, // 设置COM输出扫描方向
        0x00, // 设置低列地址
        0x10, // 设置高列地址
        0x40, // 设置起始行地址
        0x81, // 设置对比度控制
        0xFF, // 对比度值
        0xA1, // 设置段重映射
        0xA6, // 正常/反转显示
        0xA8, // 设置多路复用率
        0x3F, // 1/64占空比
        0xD3, // 设置显示偏移
        0x00, // 无偏移
        0xD5, // 设置显示时钟分频比
        0xF0, // 设置时钟为最大值
        0xD9, // 设置预充电周期
        0x22, // 预充电周期
        0xDA, // 设置COM引脚硬件配置
        0x12, // 交替COM配置
        0xDB, // 设置VCOMH
        0x20, // 0.77xVcc
        0x8D, // 设置充电泵
        0x14, // 开启充电泵
        0xAF  // 开启显示
    };
    
    // 发送初始化序列
    for(uint16_t i = 0; i < sizeof(init_sequence); i++) {
        if(i > 0 && (i % 5 == 0)) Delay_us(50);  // 每5个命令后稍作延迟
        OLED_WriteCommand(init_sequence[i]);
    }
    
    // 4. 清屏
    OLED_Clear();
    Delay_ms(100);
}


/**
  * @brief  OLED显示遥测数据
  * @param  无
  * @retval 无
  */
void OLED_ShowTelemetryData(void)
{
    // 清屏避免残影
    OLED_Clear();
    
    // 1. 显示赛道偏差 (第一行)
    OLED_ShowString(1, 1, "Dev:");
    OLED_ShowSignedNum(1, 5, g_telemetryData.trackDeviation, 4);
    
    // 2. 显示电感ADC值 (第二行，显示前3路)
    OLED_ShowString(2, 1, "ADC:");
    OLED_ShowNum(2, 5, g_telemetryData.adcValues[0], 4); // L2
    OLED_ShowString(2, 10, ",");
    OLED_ShowNum(2, 11, g_telemetryData.adcValues[1], 4); // L1
    OLED_ShowString(2, 16, ",");
    OLED_ShowNum(2, 17, g_telemetryData.adcValues[2], 4); // M
    
    // 3. 显示速度数据 (第三行)
    OLED_ShowString(3, 1, "Spd:");
    OLED_ShowNum(3, 5, (uint32_t)g_telemetryData.targetSpeed, 3); // 目标速度
    OLED_ShowString(3, 9, "/");
    OLED_ShowNum(3, 10, (uint32_t)g_telemetryData.currentSpeed, 3); // 当前速度
    
    // 4. 显示姿态角 (第四行)
    OLED_ShowString(4, 1, "Yaw:");
    OLED_ShowNum(4, 5, (uint32_t)(g_telemetryData.yawAngle + 0.5f), 3); // 四舍五入显示整数
}
