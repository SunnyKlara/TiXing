/* ---------------- EventBus.h ---------------- */
/* 事件总线 + 消息队列：观察者模式 + 中断安全队列 */
#ifndef __EVENTBUS_H
#define __EVENTBUS_H
/*
编码器逆时针转:EVENT 1
编码器顺时针转:EVENT 2
按键中断BACK:EVENT 3
按键中断OK:EVENT 4
*/
/* ---------------- 事件定义 ---------------- */
typedef enum {
    EV_NONE = 0,	// 显式指定为 0
	EV_ENC_CW,		// 逆时针转,编号自动 = 0 + 1 = 1
	EV_ENC_CCW,		//顺时针转
	EV_KEY_BACK_RAW,		//PB11未消抖信号
	EV_KEY_BACK_CLICK,		//PB11单击
	EV_KEY_BACK_LONGPRESS,	//PB11长按
    EV_KEY_OK_RAW,			// PB0未消抖信号
	EV_KEY_OK_CLICK,		//PB0单击
	EV_KEY_OK_LONGPRESS,	//PB0长按
	EV_TIMER_OLED_UPDATE,	//定时器屏幕刷新任务
	EV_TIMER_TASK,	//定时器任务
    EV_MAX			//编号最大值
} EventType;

/* ---------------- 支持的订阅函数参数类型 ---------------- */
typedef enum {
    HANDLER_VOID,      // void (*)(void)
    HANDLER_INT        // void (*)(int)
} HandlerType;

/* ---------------- 万能处理器：函数 + 参数 + 类型标记 ---------------- */
typedef struct {
    void (*Handler)(void);    // 存储时强转为void(*)(void)
    char param;                // 预绑定的int参数（无参函数不用）
    HandlerType type;         // 标记真实类型,根据不同类型判定分发逻辑
} EventHandler_t;

/* ---------------- 消息队列的单体元素 ---------------- */
typedef struct {
   EventType type;
} Event_t;

/* ---------------- 当前事件 ---------------- */
extern volatile EventType CurrentEvent;	//永远指向当前事件

/* ---------------- 对外函数接口 ---------------- */
void EventBus_Init(void);
void EventBus_Subscribe(EventType ev, void (*handler)(void));	//订阅者不带参数
void EventBus_SubscribeInt(EventType ev, void (*handler)(int), char param);	//订阅者带参数int
void EventBus_Publish(EventType ev);
void EventBus_Unsubscribe(EventType ev, void (*handler)(void));
void EventBus_UnsubscribeAll(EventType ev);		//取消所有事件的订阅
uint8_t EventBus_IsVoid(EventType ev);	//查询该事件ev是否没有观察者对象
void EventQueue_Init(void);				//初始化事件队列
u8 EventQueue_IsEmpty(void);			//检测事件队列是否为空,0空，1非空
u8 EventQueue_Enqueue(EventType ev);      // 中断安全
EventType EventQueue_Dequeue(void);         // 主循环调用

#endif
