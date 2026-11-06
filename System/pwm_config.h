#ifndef PWM_CONFIG_H_
#define PWM_CONFIG_H_

#include "main.h"

void PWM_Init(void);
void PWM_SetDuty(uint8_t channel, uint16_t duty);

#endif /* PWM_CONFIG_H_ */
