/* ---------------- EventObserver.c ---------------- */
/* ---------------- 事件观察者框架,解耦复杂逻辑的事件响应 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include "OLED.h"
#include "Animation.h"
#include "MyTimer.h"
#include "delay.h"
#include "Key.h"
#include "Image.h"
#include "TIMESET.h"
#include "MyRTC.h"


#include "EventObserver.h"



#define MAX_SUBSCRIBERS 4
static void (*handlers[EV_MAX][MAX_SUBSCRIBERS])(void);	//函数指针二维数组,装载函数总数=事件总数*最大观察者数
static uint8_t handler_count[EV_MAX];	//存放事件EV的"订阅者"数量

/* ---------------- 让动作函数订阅事件ev,当发布ev事件时调用订阅者 ---------------- */
void EventBus_Subscribe(EventType ev, void (*handler)(void)) 
{
    if (ev < EV_MAX && handler_count[ev] < MAX_SUBSCRIBERS) 
	{
        handlers[ev][handler_count[ev]++] = handler;
    }
}

/* ---------------- 发布事件ev,遍历执行直至存在的订阅者函数全部执行 ---------------- */
void EventBus_Publish(EventType ev) {
    for (int i = 0; i < handler_count[ev]; i++) 
	{
        handlers[ev][i]();
    }
}

/* ---------------- 取消订阅：从事件 ev 中移除指定的 handler订阅函数 ---------------- */
void EventBus_Unsubscribe(EventType ev, void (*handler)(void)) 
{
    if (ev >= EV_MAX) return;
    
    for (int i = 0; i < handler_count[ev]; i++) 
    {
        if (handlers[ev][i] == handler) 
        {
            // 找到后,把后面的元素前移一位
            for (int j = i; j < handler_count[ev] - 1; j++) 
            {
                handlers[ev][j] = handlers[ev][j + 1];
            }
            handler_count[ev]--;  // 订阅者数量减1
            break;  // 假设同一个 handler 不会重复订阅
        }
    }
}


