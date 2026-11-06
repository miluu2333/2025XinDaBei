#include "stm32f10x.h"
#include "Key.h"
#include "main.h" 
#include "Delay.h" 

#define KEY_DEBOUNCE_TIME 20    // 消抖时间 (ms)
#define KEY_LONG_PRESS_TIME 1000 // 长按判断时间 (ms)

// 按键状态结构体（用于非阻塞式处理）
typedef struct {
    uint8_t state;           // 当前状态
    uint32_t press_tick;     // 按下时间戳
    uint8_t debounce_state;  // 消抖状态
    uint32_t debounce_tick;  // 消抖时间戳
} KeyStatus_t;

// 按键状态变量（静态，仅在模块内可见）
static KeyStatus_t key_status[2] = {0};

/**
  * 函    数：按键初始化
  * 参    数：无
  * 返 回 值：无
  */
void Key_Init(void)
{
    // 开启 GPIO 时钟 (PA 和 PB)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 初始化 KEY1 (PA5)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // 上拉输入
    GPIO_InitStructure.GPIO_Pin = KEY1_PIN;       // PA5
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(KEY1_PORT, &GPIO_InitStructure);
    
    // 初始化 KEY2 (PB3)
    // 注意：PB3 默认是 JTAG 引脚 (JTDO)，如果不禁用 JTAG，无法作为普通 GPIO 使用
    // 我们在 main.c 的 GPIO_Configuration 中已经调用了 GPIO_Remap_SWJ_JTAGDisable
    // 所以这里直接初始化即可
    GPIO_InitStructure.GPIO_Pin = KEY2_PIN;       // PB3
    GPIO_Init(KEY2_PORT, &GPIO_InitStructure);
    
    // 初始化按键状态
    for (uint8_t i = 0; i < 2; i++) {
        key_status[i].state = 0;
        key_status[i].press_tick = 0;
        key_status[i].debounce_state = 1; // 初始为释放状态
        key_status[i].debounce_tick = 0;
    }
}

/**
  * 函    数：按键获取键码（非阻塞式）
  * 参    数：无
  * 返 回 值：按下按键的键码值，范围：0~2，返回0代表没有按键按下
  * 注意事项：非阻塞式实现，不会暂停程序运行
  */
uint8_t Key_GetNum(void)
{
    uint8_t KeyNum = 0;
    
    // 非阻塞式按键检测（这里简化实现，实际应该使用状态机）
    // 检测 KEY1 (PA5)
    if (GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) == 0)
    {
        Delay_ms(20); // 临时使用阻塞式消抖，后续可改为非阻塞
        if (GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) == 0) // 再次确认
        {
            KeyNum = 1;
            // 等待松手（阻塞式，后续可改为非阻塞）
            while (GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) == 0); 
        }
    }
    
    // 检测 KEY2 (PB3) - 用于切换菜单
    if (GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN) == 0)
    {
        Delay_ms(20); // 临时使用阻塞式消抖，后续可改为非阻塞
        if (GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN) == 0)
        {
            KeyNum = 2;
            // 等待松手（阻塞式，后续可改为非阻塞）
            while (GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN) == 0);
        }
    }
    
    return KeyNum;
}

/**
  * 函    数：按键获取状态（非阻塞式，支持长按检测）
  * 参    数：key_num - 按键编号 (1或2)
  * 返 回 值：按键状态 (KEY_NONE, KEY_CLICK, KEY_LONG_PRESS)
  * 注意事项：非阻塞式实现，需要在主循环中频繁调用
  */
KEY_STATE Key_GetState(uint8_t key_num)
{
    if (key_num < 1 || key_num > 2) {
        return KEY_NONE;
    }
    
    uint8_t key_idx = key_num - 1;
    KEY_STATE state = KEY_NONE;
    GPIO_TypeDef* port = (key_num == 1) ? KEY1_PORT : KEY2_PORT;
    uint16_t pin = (key_num == 1) ? KEY1_PIN : KEY2_PIN;
    uint32_t current_tick = Get_SysTick_ms();
    
    // 读取当前按键电平
    uint8_t current_level = GPIO_ReadInputDataBit(port, pin) == 0 ? 0 : 1;
    
    // 第一步：消抖处理（非阻塞）
    if (current_level != key_status[key_idx].debounce_state) {
        // 电平变化，开始消抖计时
        key_status[key_idx].debounce_tick = current_tick;
        key_status[key_idx].debounce_state = current_level;
    } else {
        // 电平稳定
        if (Delay_ms_NonBlock(key_status[key_idx].debounce_tick, KEY_DEBOUNCE_TIME)) {
            // 消抖完成，处理按键状态
            if (current_level == 0) { // 按键按下
                if (key_status[key_idx].state == 0) { // 首次按下
                    key_status[key_idx].state = 1;
                    key_status[key_idx].press_tick = current_tick;
                } else if (key_status[key_idx].state == 1) { // 持续按下
                    // 检查是否达到长按时间
                    if (Delay_ms_NonBlock(key_status[key_idx].press_tick, KEY_LONG_PRESS_TIME)) {
                        key_status[key_idx].state = 2; // 进入长按状态
                        state = KEY_LONG_PRESS;
                    }
                }
            } else { // 按键释放
                if (key_status[key_idx].state == 1) { // 按下后释放，未达到长按
                    state = KEY_CLICK;
                }
                // 重置状态
                key_status[key_idx].state = 0;
            }
        }
    }
    
    return state;
}
