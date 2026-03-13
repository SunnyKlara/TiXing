#ifndef __MYTIMER_H
#define __MYTIMER_H

void Timer_Init(void);
extern uint8_t FPS;
typedef struct TimerTask	//控制给定时器定时刷写屏幕等操作
{
	volatile uint8_t UpdateTask;
}TimerTask;
void Timer_SetPrescaler(uint16_t prescaler);
extern TimerTask TaskTimer;
#endif

