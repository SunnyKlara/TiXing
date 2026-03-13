/* ---------------- FPSSET.c ---------------- */
/* ---------------- 设置菜单元素平移缩放动画总帧数 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include "OLED.h"
#include "Animation.h"
#include "MyTimer.h"
#include "delay.h"
#include "Key.h"
#include "Image.h"
#include "Action.h"
#include "FPSSET.h"
typedef enum 
{
	FPSSET_SWITCH,
	FPSSET_FRAMESET,
	FPSSET_PAUSE,
	FPSSET_OVER
}FPSSET_STATE;
static FPSSET_STATE STATE=FPSSET_SWITCH;

void FPSSET_Enter()
{
	int8_t i;
	ShowMenu(1);
	memcpy(ImageTemp,DRAW_BUF, sizeof(DRAW_BUF));
	for(i=-30;i<10;i+=2)
	{
		memcpy(DRAW_BUF,ImageTemp, sizeof(DRAW_BUF));
		OLED_FillRect(i-2,25-2,40+4,80+4,0);
		OLED_FillRect(i-1,25-1,40+2,80+2,1);
		OLED_FillRect(i,25,40,80,0);
		OLED_Update();
	}
	OLED_ShowString(12,36,"OFF",menuOptions.enableAnimation,16,8);
	OLED_ShowString(12,75,"ON",!menuOptions.enableAnimation,16,8);
	OLED_ShowString(30,30,"Frame:",1,16,8);
	OLED_ShowNum(30,80,menuOptions.AnimationFrame,3,16,8,1);
	OLED_Update();
	
	ActionKey_Init(FPSSET_KeyAction);
	//Delay_ms(100);
	TaskTimer.UpdateTask=1;	//定时器周期刷写OLED功能
}
static u8 temp=1;
void FPSSET_KeyAction(uint8_t Event)
{
	temp=menuOptions.enableAnimation;
	switch(STATE)
	{
		case FPSSET_SWITCH:
			
			if (Event == 1)//反转
			{
				if (menuOptions.enableAnimation)//如果启用动画,enableAnimation==1
				{
					OLED_ShowString(12,36,"OFF",0,16,8);//
					OLED_ShowString(12,75,"ON",1,16,8);//
					menuOptions.AnimationFrame = 0;
				}
			
			}
			else if(Event == 2)
			{
				if (~menuOptions.enableAnimation)//如果没启用动画,enableAnimation==0
				{
					OLED_ShowString(12,36,"OFF",1,16,8);//白底黑字
					OLED_ShowString(12,75,"ON",0,16,8);//黑底白字
					menuOptions.AnimationFrame = 1;
				}
			}
			else if (Event == 3)
			{
				TaskTimer.UpdateTask=0;
				
				for(int8_t i=10;i>-48;i-=2)
				{
					memcpy(OLED_BUF[buf_idx],ImageTemp, sizeof(OLED_BUF[buf_idx]));
					OLED_FillRect(i-2,25-2,40+2,80+4,0);
					OLED_FillRect(i-1,25-1,40+2,80+2,1);
					OLED_FillRect(i,25,40,80,0);
					OLED_Update();
				}
				ActionKey_Free(FPSSET_KeyAction);
			}
			else if (Event == 4)
			{
				OLED_ShowString(12,36,"OFF",1,16,8);//白底黑字
				OLED_ShowString(12,75,"ON",1,16,8);//黑底白字
				
				STATE=FPSSET_FRAMESET;
				OLED_ShowString(30,30,"Frame:",0,16,8);
			}
			break;
		case FPSSET_FRAMESET:
			if (Event == 1)
			{
				menuOptions.AnimationFrame--;
				OLED_ShowNum(30,80,menuOptions.AnimationFrame,3,16,8,1);				
			}
			else if (Event == 2)
			{
				menuOptions.AnimationFrame++;
				OLED_ShowNum(30,80,menuOptions.AnimationFrame,3,16,8,1);
			}
			else if (Event == 3 )
			{
				TaskTimer.UpdateTask=0;
				for(int8_t i=10;i>-48;i-=2)
				{
					memcpy(OLED_BUF[buf_idx],ImageTemp, sizeof(OLED_BUF[buf_idx]));
					OLED_FillRect(i-2,25-2,40+2,80+4,0);
					OLED_FillRect(i-1,25-1,40+2,80+2,1);
					OLED_FillRect(i,25,40,80,0);
					OLED_Update();
				}
				ActionKey_Free(FPSSET_KeyAction);
			}
			else if (Event == 4)
			{
				OLED_ShowString(12,36,"OFF",menuOptions.AnimationFrame,16,8,16,8);
				OLED_ShowString(12,75,"ON",!menuOptions.AnimationFrame,16,8,16,8);
				STATE=FPSSET_SWITCH;
				OLED_ShowString(30,30,"Frame:",1,16,8,16,8);
			}
	}
}
	
	
	