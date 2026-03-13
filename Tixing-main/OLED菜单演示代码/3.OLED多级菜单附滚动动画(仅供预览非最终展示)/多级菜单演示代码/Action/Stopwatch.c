/* ---------------- Stopwatch.c ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include "OLED.h"
#include "Animation.h"
#include "MyTimer.h"
#include "Stopwatch.h"
#include "Key.h"
#include "Delay.h"
#include "SelectorPhysics.h"
#include "Action.h"
/*
编码器逆时针转:EVENT 1
编码器顺时针转:EVENT 2
按键中断1:EVENT 3
按键中断2:EVENT 4
*/
/* 按键事件值 */
#define ENC_CW      1
#define ENC_CCW     2
#define KEY_BACK    3
#define KEY_OK      4
#define KEY_OK_LONGPRESS      6

typedef enum {
    STOPWATCH_READY,
    STOPWATCH_PLAYING,
    STOPWATCH_PAUSE,
    STOPWATCH_OVER
} STOPWATCH_STATE;

/*---------- 全局变量 ----------*/
static STOPWATCH_STATE state = STOPWATCH_READY;
STOPPWATCHTIME StopwatchTime=
{
	.Hour=0,
	.Min=0,
	.Sec=0,
	.Dot=0,
};
static SelectorPhysics menu_selector;
static uint8_t selected_index = 0;
/**
  * @brief  对外接口,初次进入应用调用本函数
  * @param  无
  * @retval 无
  */
void Stopwatch_Enter(void)
{
	for(u8 i=3;i<12;i++)
	{
		OLED_OffsetUpdate(i,0);	//简易动画
	}
	OLED_ClearBuf();
    state = STOPWATCH_READY;
	ActionDisplay_Init(Stopwatch_Display);	//定时器注册屏幕ui显示任务
	ActionKey_Init(Stopwatch_KeyAction);	//将应用按键函数注册到按键事件上
	Timer_SetPrescaler(menuOptions.SystemClock*1);//(72*menuOptions.SystemClock/72)	//设置定时器中断周期
    TaskTimer.UpdateTask = 1;	//屏幕定期刷新任务,周期0.1秒
	
	Selector_Init(&menu_selector, 4, 8, 24, 114); // 选项框默认选中第一项
}

/**
  * @brief  应用的按键交互,,采用状态机
  * @param  无
  * @retval 无
  */
void Stopwatch_KeyAction(uint8_t event)
{
    switch (state) {
    case STOPWATCH_READY:
        if (event == KEY_OK) { state = STOPWATCH_PLAYING; }	//ACTION_Exit();
		else if (event == KEY_BACK) 
		{
			ACTION_Exit();
		}
		else if (event == KEY_OK_LONGPRESS) 
		{
			Stopwatch_Reset();	//长按复位
		}
        break;

	case STOPWATCH_PLAYING:
		if (event == ENC_CW) {              // 顺时针 → 

		}
		else if (event == ENC_CCW) {        // 逆时针 →

		}
		else if (event == KEY_OK || event == KEY_BACK) {
			state = STOPWATCH_PAUSE;   // 两个按键都当暂停
			Show_Stopwatch();
			
			Selector_SetTarget(&menu_selector, 
			  36, 40, 
			  24, 50);	//测试代码
			
		}
		else if (event == KEY_OK_LONGPRESS) 
		{
			Stopwatch_Reset();	//长按复位
		}
		
		break;
    case STOPWATCH_PAUSE:
        if (event == KEY_OK) 
		{
			state = STOPWATCH_PLAYING; 
			Selector_SetTarget(&menu_selector, 4, 8, 24, 112); // 默认选中第一项
		}
		else if (event == KEY_BACK) 
		{
			ACTION_Exit();
		}
		else if (event == KEY_OK_LONGPRESS) 
		{
			Stopwatch_Reset();	//长按复位
		}
		break;
    case STOPWATCH_OVER:
        if (event == KEY_OK) 
		{
			state = STOPWATCH_READY;
		}
		else if (event == KEY_BACK) 
		{
			ACTION_Exit();	//长按复位
		}
        break;
    }
}
/**
  * @brief  应用的显示逻辑,由定时器调用
  * @param  无
  * @retval 无
  */
void Stopwatch_Display(void)
{
    
	OLED_ClearBuf();               // 双缓冲清 0
    switch (state) {
    case STOPWATCH_READY:
		Show_Stopwatch();
        break;

    case STOPWATCH_PLAYING:
		StopwatchTime.Dot++;
		if(StopwatchTime.Dot>=100)
		{
			StopwatchTime.Dot=0;
			StopwatchTime.Sec++;
			if(StopwatchTime.Sec>=60)
			{
				StopwatchTime.Sec=0;
				StopwatchTime.Min++;
				if(StopwatchTime.Min>=60)
				{
					StopwatchTime.Min=0;
					StopwatchTime.Hour++;
				}
			}
		}
		Show_Stopwatch();
		break;
    case STOPWATCH_PAUSE:
		Show_Stopwatch();
		state = STOPWATCH_PAUSE; 
		
		break;	
    case STOPWATCH_OVER:

        break;
    }
}

/**
  * @brief  绘制应用页面,由定时器每0.01秒更新秒表
  */
void Show_Stopwatch(void)	
{
	OLED_Printf(4,8,24,14,0,"%02d:%02d:%02d",StopwatchTime.Min,StopwatchTime.Sec,StopwatchTime.Dot);
	//
	if(state == STOPWATCH_PAUSE)	OLED_Printf(36,40,24,12,0,"STOP");
	//更新并绘制选择框
	Selector_Update(&menu_selector);
	Selector_Draw(&menu_selector);

}
/**
  * @brief  秒表时间复位
  */
void Stopwatch_Reset(void)	
{
	StopwatchTime.Hour=0;
	StopwatchTime.Sec=0;
	StopwatchTime.Min=0;
	StopwatchTime.Dot=0;
}