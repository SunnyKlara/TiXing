/* ---------------- TIMESET.c ---------------- */
/* ---------------- 时间设置页,当一行反色显示时,使用编码器调整数值,按OK键确认 ---------------- */
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
#include "Action.h"
typedef enum
{
	TIMESET_SWITCH,
	TIMESET_SETTIME,
	TIMESET_PAUSE,
	TIMESET_OVER
}TIMESET_STATE;	//状态机
static TIMESET_STATE STATE=TIMESET_SWITCH;	//初始状态

TimeElement TimeArray[6]=
{
	{
		.Inx=0,
		.Value=&MyRTC_Time[0],
		.Str="年",	//这里填写描述
	},
	
	{
		.Inx=1,
		.Value=&MyRTC_Time[1],
		.Str="月",	//这里填写描述
	},
	{
		.Inx=2,
		.Value=&MyRTC_Time[2],
		.Str="日",	//这里填写描述
	},
	{
		.Inx=3,
		.Value=&MyRTC_Time[3],
		.Str="时",	//这里填写描述
	},
	{
		.Inx=4,
		.Value=&MyRTC_Time[4],
		.Str="分",	//这里填写描述
	},
	{
		.Inx=5,
		.Value=&MyRTC_Time[5],
		.Str="秒",	//这里填写描述	
	}
};

Contorller controller=	//总偏移=基本偏移+指针偏移 :	指的是元素位置的偏移,基本偏移单位为16行
{
	.BaseOffset=0,
	.PointerOffset=0,
	.ElementHeight=16,
	.OffsetLine=0,	//一次动画需要偏移的行数（指OLED的64行），间接用于输入LineOffset.target_output
};


static ElementOffset LineOffset;	//用于弹簧阻尼系统的控制器

int8_t BaseOffsetTemp=0;	//页面元素显示的范围是以动画结束后的实际元素总偏移决定的，因此用temp暂时储存

void LineOffset_Reset(void)
{
	  
	LineOffset.current_input=0;
	LineOffset.target_output=0;
	controller.BaseOffset+=BaseOffsetTemp;
	controller.OffsetLine=0;
	BaseOffsetTemp=0;
	/*
	//调试用
	if(controller.PointerOffset==3)
	{
		controller.BaseOffset++;
	}
	else if(controller.PointerOffset==0)
	{
		controller.BaseOffset--;
	}
	*/
	
	
	//ACTION_Exit();	//TEST
}

/**
  * @brief  初次进入应用调用本函数
  * @param  无
  * @retval 无
  */
void TIMESET_Enter()
{
	int8_t i;
	
	OLED_ClearBuf();
	ActionKey_Init(TIMESET_KeyAction);		//绑定本页面的按键函数
	ActionDisplay_Init(TIMESET_ShowUI);	//把显示函数订阅到定时器事件
	TIMESET_ShowUI();		
	ElementOffset_Init(&LineOffset,0);	//以'改变行偏移'的动画控制器初始化
	Timer_SetPrescaler(10*menuOptions.SystemClock/72);	//定时器周期控制
	TaskTimer.UpdateTask = 1;	//启动定时器更新屏幕任务
	
	
}

/**
  * @brief  应用的按键交互,,采用状态机
  * @param  无
  * @retval 无
  */
void TIMESET_KeyAction(uint8_t Event)
{
	switch(STATE)
	{
		case TIMESET_SWITCH:
			
			if (Event == 1)//反转
			{
				if(controller.PointerOffset>0) {controller.PointerOffset--;}

				else if(controller.BaseOffset+BaseOffsetTemp>0) 
				{
					ElementOffset_SetTarget(&LineOffset,controller.OffsetLine+=16,LineOffset_Reset);
					BaseOffsetTemp-=1;
				}
			
			
			}
			else if(Event == 2)
			{
				if(controller.PointerOffset<3) {controller.PointerOffset++;}
				else if(controller.BaseOffset+BaseOffsetTemp<2) 
				{
					ElementOffset_SetTarget(&LineOffset,controller.OffsetLine-=16,LineOffset_Reset);
					BaseOffsetTemp+=1;
				}
			}
			else if (Event == 3)
			{
				ACTION_Exit();	//退出页面
			}
			else if (Event == 4)
			{
				STATE=TIMESET_SETTIME;	//进入时间设置状态
				TIMESET_ShowUI();

			}
			break;
		case TIMESET_SETTIME:
			
			if (Event == 1)
			{
				(*TimeArray[controller.BaseOffset+controller.PointerOffset].Value)--;	//值减
			}
			else if (Event == 2)
			{
				(*TimeArray[controller.BaseOffset+controller.PointerOffset].Value)++;	//值增
			}
			else if (Event == 3 )
			{
				STATE=TIMESET_SWITCH;
				ACTION_Exit();
				
			}
			else if (Event == 4)
			{
				STATE=TIMESET_SWITCH;
				MyRTC_SetTime();
			}
	}
}

/**
  * @brief  画时间设置的界面UI
  * @param  无
  * @retval 无
  */

void TIMESET_ShowUI(void)
{
	OLED_ClearBuf();
	if(STATE==TIMESET_SWITCH) MyRTC_ReadTime();	//RTC读时间
	int8_t min=(controller.BaseOffset>0)?-controller.BaseOffset:0;	//用于显示屏幕上方不可见的BaseOffset个元素
	int8_t max=4-min+BaseOffsetTemp;	//限制元素的显示数量
	for(int8_t i=min;i<max;i++)			//从顶部依次显示年月日等元素
	{
		OLED_Printf((int8_t)(LineOffset.current_input/10)+16*i,0,16,16,0,"%s:%2d",(TimeArray[controller.BaseOffset+i].Str),*(TimeArray[controller.BaseOffset+i].Value));
	}
	OLED_ShowString(16*controller.PointerOffset,100,"<-",0);	//可视化指针
	if(STATE==TIMESET_SETTIME) OLED_ReverseArea(16*controller.PointerOffset,0,16,128);	//按OK按键选中后反色
	ElementOffset_Update(&LineOffset);	//更新页面滑动动画
}


