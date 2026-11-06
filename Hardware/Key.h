#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>

// 按键状态枚举
typedef enum
{
    KEY_NONE = 0,        // 无按键操作
    KEY_CLICK = 1,       // 单击
    KEY_LONG_PRESS = 2   // 长按
} KEY_STATE;

void Key_Init(void);
uint8_t Key_GetNum(void);

/**
  * 函    数：按键获取状态（支持长按检测）
  * 参    数：key_num - 按键编号 (1或2)
  * 返 回 值：按键状态 (KEY_NONE, KEY_CLICK, KEY_LONG_PRESS)
  */
KEY_STATE Key_GetState(uint8_t key_num);

#endif

