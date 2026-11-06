# GPIO引脚配置汇总表

## 说明
以下是STM32项目中所有GPIO引脚的配置和功能汇总，基于代码分析。

## GPIO引脚汇总表

| 端口 | 引脚 | 功能 | 模式 | 速度 | 来源文件 | 备注 |
|------|------|------|------|------|----------|------|
| PA0 | - | 左电机控制A相 | 推挽输出 | 50MHz | main.c | GPIO_Configuration |
| PA0 | - | ADC赛道传感器 | 模拟输入 | - | main.c | GPIO_Configuration |
| PA1 | - | 左电机控制A相 | 推挽输出 | 50MHz | main.c | GPIO_Configuration |
| PA1 | - | LED1 | 推挽输出 | 50MHz | LED.c | 高电平关闭 |
| PA2 | - | 左电机控制A相 | 推挽输出 | 50MHz | main.c | GPIO_Configuration |
| PA2 | - | LED2 | 推挽输出 | 50MHz | LED.c | 高电平关闭 |
| PA3 | - | 左电机控制A相 | 推挽输出 | 50MHz | main.c | GPIO_Configuration |
| PA3 | - | ADC赛道传感器 | 模拟输入 | - | main.c | GPIO_Configuration |
| PA4 | - | 右电机控制B相 | 推挽输出 | 50MHz | main.c | GPIO_Configuration |
| PA4 | - | ADC赛道传感器 | 模拟输入 | - | main.c | GPIO_Configuration |
| PA5 | - | 右电机控制B相 | 推挽输出 | 50MHz | main.c | GPIO_Configuration |
| PA5 | - | ADC赛道传感器 | 模拟输入 | - | main.c | GPIO_Configuration |
| PA5 | KEY1_PIN | 按键1 | 上拉输入 | 50MHz | Key.c | 来自Hardware/main.h |
| PA6 | - | 右电机控制B相 | 推挽输出 | 50MHz | main.c | GPIO_Configuration |
| PA6 | - | ADC赛道传感器 | 模拟输入 | - | main.c | GPIO_Configuration |
| PA7 | - | 右电机控制B相 | 推挽输出 | 50MHz | main.c | GPIO_Configuration |
| PA8 | - | 超声波Trig | 推挽输出 | 50MHz | main.c | GPIO_Configuration |
| PA8 | PWM_LEFT_PIN | PWM左电机 | 复用推挽 | 50MHz | pwm_config.c | 来自Hardware/main.h |
| PA9 | - | 超声波Echo | 浮空输入 | - | main.c | GPIO_Configuration |
| PA9 | - | USART1 TX | 复用推挽 | 50MHz | main.c | GPIO_Configuration |
| PA10 | - | USART1 RX | 浮空输入 | - | main.c | GPIO_Configuration |
| PA11 | PWM_RIGHT_PIN | PWM右电机 | 复用推挽 | 50MHz | pwm_config.c | 来自Hardware/main.h |
| PB0 | - | 按键1 | 上拉输入 | - | main.c | GPIO_Configuration |
| PB1 | - | 按键2 | 上拉输入 | - | main.c | GPIO_Configuration |
| PB3 | KEY2_PIN | 按键2 | 上拉输入 | 50MHz | Key.c | 来自Hardware/main.h |
| PB6 | - | 左电机编码器 | 浮空输入 | - | main.c | GPIO_Configuration |
| PB6 | - | I2C1 | 复用开漏 | 50MHz | main.c | GPIO_Configuration |
| PB7 | - | 左电机编码器 | 浮空输入 | - | main.c | GPIO_Configuration |
| PB7 | - | I2C1 | 复用开漏 | 50MHz | main.c | GPIO_Configuration |
| PB8 | - | 右电机编码器 | 浮空输入 | - | main.c | GPIO_Configuration |
| PB9 | - | 右电机编码器 | 浮空输入 | - | main.c | GPIO_Configuration |
| PB10 | - | OLED SDA | 开漏输出 | 50MHz | OLED.c | OLED_I2C_Init |
| PB11 | - | OLED SCL | 开漏输出 | 50MHz | OLED.c | OLED_I2C_Init |
| PB15 | - | 蜂鸣器 | 推挽输出 | 50MHz | main.c | GPIO_Configuration |

## 注意事项

1. **引脚复用冲突**：
   - PA0: 同时用于左电机控制和ADC赛道传感器
   - PA8: 同时用于超声波Trig和PWM左电机
   - PB6: 同时用于左电机编码器和I2C1
   - PB7: 同时用于左电机编码器和I2C1

2. **按键定义不一致**：
   - main.c中定义PB0/PB1为按键1/2
   - Key.c中使用PA5/PB3为按键1/2（来自Hardware/main.h）

3. **JTAG引脚**：
   - PB3默认是JTAG引脚(JTDO)，需要禁用JTAG才能作为普通GPIO使用

4. **功能重复**：
   - 电机控制同时使用了普通GPIO输出(PA0-PA7)和PWM引脚(PA8, PA11)

## 文件来源说明

- **main.c**: `/User/main.c` 中的GPIO_Configuration函数
- **OLED.c**: `/Hardware/OLED.c` 中的OLED_I2C_Init函数
- **LED.c**: `/Hardware/LED.c` 中的LED_Init函数
- **Key.c**: `/Hardware/Key.c` 中的Key_Init函数
- **pwm_config.c**: `/System/pwm_config.c` 中的PWM_Init函数
- **Hardware/main.h**: 包含按键和PWM的宏定义

## 建议

1. 解决引脚复用冲突，确保每个引脚只用于一个功能
2. 统一按键的定义，避免混淆
3. 检查电机控制的实现方式，确定是使用普通GPIO还是PWM
4. 为每个硬件模块创建独立的配置文件，提高代码可读性和维护性