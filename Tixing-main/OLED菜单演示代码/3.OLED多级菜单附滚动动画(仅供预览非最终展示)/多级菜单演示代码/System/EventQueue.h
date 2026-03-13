/* ---------------- EventObserver.h ---------------- */

#ifndef __EVENTOBSERVER_H
#define __EVENTOBSERVER_H
/*
编码器逆时针转:EVENT 1
编码器顺时针转:EVENT 2
按键中断1:EVENT 3
按键中断2:EVENT 4
*/

typedef enum {
    EV_NONE = 0,	// 显式指定为 0
	EV_ENC_CW,		// 逆时针转,编号自动 = 0 + 1 = 1
	EV_ENC_CCW,		//顺时针转
	EV_KEY_BACK,	//PB11
    EV_KEY_OK,		// PB0
    EV_MAX		//编号5
} EventType;

void EventBus_Init(void);
void EventBus_Subscribe(EventType ev, void (*handler)(void));
void EventBus_Publish(EventType ev);
void EventBus_Unsubscribe(EventType ev, void (*handler)(void));

#endif
