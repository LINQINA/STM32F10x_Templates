#ifndef _DevicesTimer_H_
#define _DevicesTimer_H_

#include "stm32f1xx.h"

extern TIM_HandleTypeDef g_timer3_initpara;
extern TIM_HandleTypeDef g_timer6_initpara;

int8_t cTimer3Init(void);
int8_t cTimer6Init(void);

#endif

