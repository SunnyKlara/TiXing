/* ---------------- Action.c ---------------- */
/* ---------------- 应用层的公共接口,应用可以使用声明的公共函数 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include "OLED.h"
#include "Animation.h"
#include "MyTimer.h"
#include "Clock.h"
#include "Key.h"
#include "MyRTC.h"
#include "EventBus.h"
#include "Action.h"
#include "HardI2C.h"
/*
编码器逆时针转:EVENT 1
编码器顺时针转:EVENT 2
按键中断1:EVENT 3
按键中断2:EVENT 4
*/
/**
  * @brief  应用的按键绑定初始化
  * @param  KeyAction按键动作函数
  * @retval 无
  */
void ActionKey_Init(void (*KeyAction)(uint8_t))
{
	EventBus_Unsubscribe(EV_KEY_BACK_CLICK,MenuBack);
	EventBus_Unsubscribe(EV_KEY_OK_CLICK,MenuForward);
	EventBus_Unsubscribe(EV_ENC_CW, Left_Shift);	     //订阅默认函数:当逆旋键判定时
	EventBus_Unsubscribe(EV_ENC_CCW, Right_Shift);       //订阅默认函数:当顺旋判定时
	EventBus_SubscribeInt(EV_ENC_CW,KeyAction,1);
	EventBus_SubscribeInt(EV_ENC_CCW,KeyAction,2);
	EventBus_SubscribeInt(EV_KEY_BACK_CLICK,KeyAction,3);
	EventBus_SubscribeInt(EV_KEY_OK_CLICK,KeyAction,4);
	EventBus_SubscribeInt(EV_KEY_OK_LONGPRESS,KeyAction,6);
}

/**
  * @brief  释放指定的按键动作,并回归默认菜单的按键操作
  * @param  KeyAction按键动作函数
  * @retval 无
  */
void ActionKey_Free(void (*KeyAction)(uint8_t))
{
	EventBus_Unsubscribe(EV_KEY_BACK_CLICK,(void(*)(void))KeyAction);
	EventBus_Unsubscribe(EV_KEY_OK_CLICK,(void(*)(void))KeyAction);
	EventBus_Unsubscribe(EV_ENC_CW, (void(*)(void))KeyAction);	     //订阅默认函数:当逆旋键判定时
	EventBus_Unsubscribe(EV_ENC_CCW, (void(*)(void))KeyAction);       //订阅默认函数:当顺旋判定时
	EventBus_Unsubscribe(EV_KEY_OK_LONGPRESS,(void(*)(void))KeyAction);       //订阅默认函数:当顺旋判定时
	EventBus_Subscribe(EV_ENC_CW,Left_Shift);
	EventBus_Subscribe(EV_ENC_CCW,Right_Shift);
	EventBus_Subscribe(EV_KEY_BACK_CLICK,MenuBack);
	EventBus_Subscribe(EV_KEY_OK_CLICK,MenuForward);
}
/**
  * @brief  恢复菜单的默认按键操作
  * @retval 无
  */
void ActionKey_Default(void)
{
	EventBus_UnsubscribeAll(EV_KEY_BACK_CLICK);
	EventBus_UnsubscribeAll(EV_KEY_OK_CLICK);
	EventBus_UnsubscribeAll(EV_ENC_CW);	    	 //订阅默认函数:当逆旋键判定时
	EventBus_UnsubscribeAll(EV_ENC_CCW);      	 //订阅默认函数:当顺旋判定时
	EventBus_Subscribe(EV_ENC_CW,Left_Shift);
	EventBus_Subscribe(EV_ENC_CCW,Right_Shift);
	EventBus_Subscribe(EV_KEY_BACK_CLICK,MenuBack);
	EventBus_Subscribe(EV_KEY_OK_CLICK,MenuForward);
}
/**
  * @brief  将app的display函数订阅到定时器EV_TIMER_TASK事件中
  * @param  Display应用内显示函数
  * @retval 无
  */
void ActionDisplay_Init(void (*Display)(void))
{
	EventBus_Subscribe(EV_TIMER_TASK,Display);
}
/**
  * @brief  定时器EV_TIMER_TASK事件注销所有观察者
  * @retval 无
  */
void ActionDisplay_Free()
{
	EventBus_UnsubscribeAll(EV_TIMER_TASK);
}

//每个应用公共的退出方法
void ACTION_Exit(void)
{
	TaskTimer.UpdateTask = 0;	//关闭屏幕刷新任务
	ActionDisplay_Free();		//释放定时器的app显示逻辑
	ActionKey_Default();	//按键回到菜单默认状态
	for(u8 i=6;i<13;i++)
	{
		OLED_OffsetUpdate(-i,0);	//加速度
	}
	MenuDown(3);	//动画
	ShowMenu(1);	//显示菜单
}


void Power_Init(void)		//统一退出接口
{
	
	RCC_HSEConfig(RCC_HSE_ON);  //中断造成卡死是因为如果在 Systick、DMA、I2C 中断上下文里调降频，中断返回时系统时钟已经被切掉
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_5);
	uint32_t t = 0;
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET && ++t < 0x5000)
	{
		if (t >= 0x5000) 
		{
			break;
		}
	}
	FLASH_SetLatency(FLASH_Latency_2);
	RCC_PLLCmd(ENABLE);
	
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	RCC_HSICmd(DISABLE);
	
	
	SystemCoreClock = 40000000;
	SystemCoreClockUpdate();
}


void StopMode_Enter(void)		//
{
	RCC_HSICmd(ENABLE);
	uint32_t t = 0;
	while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET && ++t < 0x1000)
	{
		if (t >= 0x1000) 
		{
			break;
		}
	}
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);
	RCC_PLLCmd(DISABLE);
	RCC_HSEConfig(RCC_HSE_OFF);              
	SystemCoreClock = 8000000;
	SystemCoreClockUpdate();	
	menuOptions.AnimationFrame = 0;
}


/*
钩子调用链(已抛弃):按键按下
  → 触发 EXTI 中断
    → 进入 EXTI0_IRQHandler / EXTI15_10_IRQHandler
      → 调用 直接设置 event
        → 触发 actionHook = CLOCK_KeyAction(event)
          → 在 CLOCK_KeyAction 中调用 动作函数()

事件驱动链(已采用):按键按下
  → 触发EXTI中断,发送未处理raw按键信号事件,
    → 进入 滴答定时器(systick)中断激活定时轮询按键状态
      → 轮询并累计按键按下时长,判断是否为抖动,短按,长按逻辑
        → 为不同逻辑分发事件,事件入队并交由主循环处理
          → 初始化,重置PressDuration,等参数,关闭定时器中断
*/