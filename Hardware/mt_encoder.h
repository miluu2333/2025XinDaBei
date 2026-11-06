/*
 * mt_encoder.h
 *
 *  Created on: 2025-08-01
 *      Author: XF Team
 */

#ifndef MT_ENCODER_H_
#define MT_ENCODER_H_

#include <stdint.h>
#include <stdbool.h>
#include "main.h"

// 编码器通道定义
typedef enum {
    ENCODER_LEFT = 0,
    ENCODER_RIGHT,
    ENCODER_MAX
} EncoderChannel_e;

// 函数声明
void Encoder_Init(void);
void Encoder_Update(void);
int32_t Encoder_GetCount(uint8_t encoder);
float Encoder_GetSpeed(uint8_t encoder);

#endif /* MT_ENCODER_H_ */
