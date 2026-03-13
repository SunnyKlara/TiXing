/* ---------------- EventBus.c ---------------- */
/* ---------------- 事件观察者+消息队列框架,二者协同解耦复杂逻辑的事件响应 ---------------- */
/* 事件总线 + 消息队列：观察者模式 + 中断安全队列 */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include "OLED.h"
//#include "Animation.h"
#include "MyTimer.h"
#include "delay.h"
#include "Key.h"
#include "Image.h"
#include "MyRTC.h"
#include <stdint.h>
#include "EventBus.h"

/* ----------------  ----------------观察者订阅与发布 ---------------- ---------------- */

#define MAX_SUBSCRIBERS 4	//最大观察者数量
static EventHandler_t handlers[EV_MAX][MAX_SUBSCRIBERS];	//处理器二维数组(记录不同事件记载的观察者,以及观察者的void/int参数信息),装载函数总数=事件总数*最大观察者数
//static void (*handlers[EV_MAX][MAX_SUBSCRIBERS])(void);	//函数指针二维数组,装载函数总数=事件总数*最大观察者数
static uint8_t handler_count[EV_MAX];	//存放每个事件EV的已有"订阅者"数量
/* ---------------- 让动作函数订阅事件ev,当发布ev事件时调用订阅者 ---------------- */
void EventBus_Subscribe(EventType ev, void (*handler)(void)) 
{
	
	  if (ev < EV_MAX && handler_count[ev] < MAX_SUBSCRIBERS) 
	{
		handlers[ev][handler_count[ev]].Handler = handler;	//该ev事件的函数指针注册订阅者
		handlers[ev][handler_count[ev]].type = HANDLER_VOID;	//并把订阅者的参数信息标为void
		handler_count[ev]++;	//该ev事件的已有"订阅者"数量自增
    }
}

// 订阅带int参数的函数
void EventBus_SubscribeInt(EventType ev, void (*handler)(int), char param) {
    if (ev < EV_MAX && handler_count[ev] < MAX_SUBSCRIBERS)
	{
		handlers[ev][handler_count[ev]].Handler = (void (*)(void))handler; // 强制转换存储
		handlers[ev][handler_count[ev]].param = param;  // 绑定参数
		handlers[ev][handler_count[ev]].type = HANDLER_INT;
		handler_count[ev]++;
	}
}

// ---------------- 发布事件ev,遍历执行直至存在的订阅者函数全部执行 ---------------- */

void EventBus_Publish(EventType ev) {
    for (int i = 0; i < handler_count[ev]; i++) 
	{
		if (handlers[ev][i].type == HANDLER_VOID) 
		{
			
			(handlers[ev][i].Handler)();          // 转回无参调用
			
		} 
		else 
		{
			((void (*)(int))handlers[ev][i].Handler)(handlers[ev][i].param);   // 强制转换int类型,转回带参调用
		}
    }
	
}


// ---------------- 取消订阅：从事件 ev 中移除指定的 handler订阅函数 ---------------- */
void EventBus_Unsubscribe(EventType ev, void (*handler)(void)) 
{
    if (ev >= EV_MAX) return;
    
    for (int i = 0; i < handler_count[ev]; i++) 
    {
        if (handlers[ev][i].Handler == handler) 
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
//取消订阅事件ev的所有观察者
void EventBus_UnsubscribeAll(EventType ev)
{
	handler_count[ev]=0;
}
//检验事件ev是否观察者为空
uint8_t EventBus_IsVoid(EventType ev)
{
	if(handlers[ev][0].Handler)
	{
		return 1;
	}
	else return 0;
}

/* ----------------  ----------------消息队列入队与出队管理 ---------------- ---------------- */

#define EVENT_QUEUE_SIZE 3  				// 最大事件数量
static Event_t queue[EVENT_QUEUE_SIZE];		//定义事件队列(本质是数组)
volatile EventType CurrentEvent=EV_NONE;	//全局变量,指向当前正在处理的事件

static volatile uint8_t head = 0;   	// 入队位置
static volatile uint8_t tail = 0;   	// 出队位置

//环形队列初始化
void EventQueue_Init(void) 
{
    head = 0;
    tail = 0;
}

//检验事件队列是否为空
u8 EventQueue_IsEmpty(void) 
{
    return head == tail;
}


// 在中断中调用，必须保证原子性
u8 EventQueue_Enqueue(EventType ev) 
{
    __disable_irq();  // 关中断
	
    uint8_t next = (head + 1) % EVENT_QUEUE_SIZE;
    if (next == tail) 
	{
        // 队列满，丢弃事件（或可加日志）
        __enable_irq();
        return 0;
    }
    queue[head].type = ev;
    head = next;

    __enable_irq();//开中断
    return 1;
}

// 在主循环中调用，无需关中断（只有主循环 Dequeue）
EventType EventQueue_Dequeue(void) 
{
    if (head == tail) 
	{
        return EV_NONE; // 安全兜底
    }
    EventType ev = queue[tail].type;
	CurrentEvent=ev;	//更新当前正在处理的事件,本代码搭配滴答中断检测按键消抖使用
    tail = (tail + 1) % EVENT_QUEUE_SIZE;	//尾自增
    return ev;
}

