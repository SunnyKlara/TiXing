/* ---------------- SystemSET.c ---------------- */
/* ---------------- 系统设置页,提供调整主频,查看信息等功能 ---------------- */
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
#include <stdlib.h>		//申请内存
#include "SystemSET.h"
#include "SystemSET_Pages.h"

typedef enum 
{
	SYSTEMSET_HOMEPAGE,	//首页
	SYSTEMSET_CHILDPAGE,	//子页
	SYSTEMSET_THIRDPAGE,	//第三页
	SYSTEMSET_OVER		//其他页
}SYSTEMSET_STATE;

static SYSTEMSET_STATE STATE=SYSTEMSET_HOMEPAGE;

SystemElement HomePage[5]=
{
	{
		.Inx=0,
		.Value=0,
		.Str="电源管理",	//这里填写描述
		.ChildPage=PowerPage,
		.Icon=Icon_Batterry,
	},
	
	{
		.Inx=1,
		.Value=0,
		.Str="重启机器",	//这里填写描述
		.Action=NVIC_SystemReset,
		.Icon=Icon_Restart,
	},
	{
		.Inx=2,
		.Value=0,
		.Str="显示设置",	//这里填写描述
		.ChildPage=DisplayPage,
		.Icon=Icon_Display,
	},
	{
		.Inx=3,
		.Value=0,
		.Str="关于设备",	//这里填写描述
		.ChildPage=AboutPage,
		.Icon=Icon_About,
	},
	{
		.Inx=255,
		.Value=0,
		.Str="定时设置",	//这里填写描述
		.Icon=Icon_Clock,
	}
};

PointerContorller Pointer=
{
	.CurrentPage=HomePage,//指针初始化指向首页
	.BaseOffset=0,
	.PointerOffset=0,
	.ReversePos=255,	//255代表不反色任意行,默认不反色
};
	
/**
  * @brief  初次进入应用调用本函数
  * @param  无
  * @retval 无
  */
void SystemSET_Enter()
{
	int8_t i,column;
	
	for(u8 i=3;i<12;i++)
	{
		column+=i;
		OLED_ClearBufArea(0,127-column,64,column);
		OLED_OffsetUpdate(0,-i);	//加速度
	}
	OLED_ClearBuf();
	SystemSET_ShowCurPage();
	ActionKey_Init(SystemSET_KeyAction);
}

/**
  * @brief  应用的按键交互,,采用状态机
  * @param  无
  * @retval 无
  */
void SystemSET_KeyAction(uint8_t Event)
{
	switch(STATE)
	{
		case SYSTEMSET_HOMEPAGE:
			
			if (Event == 1)//反转
			{
				
				if(Pointer.PointerOffset>0) {Pointer.PointerOffset--;}

				else if(Pointer.BaseOffset>0) {Pointer.BaseOffset--;}
				
				SystemSET_ShowCurPage();
				
			}
			else if(Event == 2)
			{
				
				if(Pointer.PointerOffset<2) {Pointer.PointerOffset++;}
				else if(Pointer.CurrentPage[Pointer.BaseOffset+2].Inx != 255) {Pointer.BaseOffset++;}
				SystemSET_ShowCurPage();
				
			}
			else if (Event == 3)
			{
				
				ACTION_Exit();
			}
			else if (Event == 4)
			{
				if(Pointer.CurrentPage[Pointer.BaseOffset+Pointer.PointerOffset].ChildPage)	//如果指针指向选项存在子页
				{
					Pointer.CurrentPage=Pointer.CurrentPage[Pointer.BaseOffset+Pointer.PointerOffset].ChildPage;
					Pointer.ChildContorller = (PointerContorller *)malloc(sizeof(PointerContorller));
					Pointer.ChildContorller->BaseOffset=Pointer.BaseOffset;
					Pointer.ChildContorller->PointerOffset=Pointer.PointerOffset;
					Pointer.BaseOffset=0;
					Pointer.PointerOffset=0;
					STATE=SYSTEMSET_CHILDPAGE;
					
					if(Pointer.ChildContorller->BaseOffset+Pointer.ChildContorller->PointerOffset==0)	//当进入电源管理页,代号0
					{
						if(menuOptions.SystemClock==72){Pointer.ReversePos=0;}
						else if(menuOptions.SystemClock==40){Pointer.ReversePos=1;}
						else if(menuOptions.SystemClock==24){Pointer.ReversePos=2;}
						else if(menuOptions.SystemClock==8){Pointer.ReversePos=3;}
							
					}
				}
				else if (Pointer.CurrentPage[Pointer.BaseOffset+Pointer.PointerOffset].Action)	//不存在子页，则执行当前绑定的动作函数
				{
					Pointer.CurrentPage[Pointer.BaseOffset+Pointer.PointerOffset].Action();
				}
				SystemSET_ShowCurPage();
			}
			break;
			
		case SYSTEMSET_CHILDPAGE:
			
			if (Event == 1)
			{
				if(Pointer.PointerOffset>0) {Pointer.PointerOffset--;}

				else if(Pointer.BaseOffset>0) {Pointer.BaseOffset--;}
				
				SystemSET_ShowCurPage();
			}
			else if (Event == 2)
			{
				if(Pointer.PointerOffset<2) {Pointer.PointerOffset++;}
				else if(Pointer.CurrentPage[Pointer.BaseOffset+2].Inx != 255) {Pointer.BaseOffset++;}
				SystemSET_ShowCurPage();
			}
			else if (Event == 3 )
			{
				u8 column;
				for(u8 i=1;i<25;i++)
				{
					column+=i;
					OLED_OffsetUpdate(0,5);	//加速度
					OLED_ClearBufArea(0,0,64,column);
				}
				Pointer.CurrentPage=HomePage;	//当前所在页指向其父页
				Pointer.BaseOffset=Pointer.ChildContorller->BaseOffset;			//父页恢复基础偏移
				Pointer.PointerOffset=Pointer.ChildContorller->PointerOffset;	//父页恢复指针偏移
				free(Pointer.ChildContorller);	//释放子页控制器
				STATE=SYSTEMSET_HOMEPAGE;	//更改状态机
				Pointer.ReversePos=255;	//反色重置
				SystemSET_ShowCurPage();//显示ui
			}
			else if (Event == 4)
			{
				if(Pointer.CurrentPage[Pointer.BaseOffset+Pointer.PointerOffset].ChildPage)	//如果指针指向选项存在子页
				{
					Pointer.CurrentPage=Pointer.CurrentPage[Pointer.BaseOffset+Pointer.PointerOffset].ChildPage;
					STATE=SYSTEMSET_THIRDPAGE;
				}
				else if (Pointer.CurrentPage[Pointer.BaseOffset+Pointer.PointerOffset].Action)
				{
					Pointer.CurrentPage[Pointer.BaseOffset+Pointer.PointerOffset].Action();
				}
				if(Pointer.CurrentPage[Pointer.BaseOffset+Pointer.PointerOffset].Action !=Display_Note ) 
				{
					SystemSET_ShowCurPage();//显示ui
				}
			}
	}
}

/**
  * @brief  显示当前状态机页面
  * @param  无
  * @retval 无
  */
void SystemSET_ShowCurPage(void)
{
	
	OLED_ClearBuf();
	
	for(u8 i=0;i<3;i++)
	{
		
		if((Pointer.CurrentPage[Pointer.BaseOffset+i].Value))
		{
			OLED_Printf(20*i+4,2,16,16,0,"%s%s",(Pointer.CurrentPage[Pointer.BaseOffset+i].Str),(Pointer.CurrentPage[Pointer.BaseOffset+i].Value));

		}
		else
		{
			if(Pointer.CurrentPage[Pointer.BaseOffset+i].Icon)
			{
				OLED_ShowImage(20*i+4,0,16,16,Pointer.CurrentPage[Pointer.BaseOffset+i].Icon,1);
			}
			if(Pointer.CurrentPage[Pointer.BaseOffset+i].Str)	OLED_Printf(20*i+4,18,18,18,0,"%s",(Pointer.CurrentPage[Pointer.BaseOffset+i].Str));
			
		}
	}

	OLED_ShowString(20*Pointer.PointerOffset+4,108,"<-",0);
	if(Pointer.ReversePos<255 && (Pointer.ReversePos-Pointer.BaseOffset)>=0) 
	{
		OLED_ReverseArea(20*(Pointer.ReversePos-Pointer.BaseOffset)+4,0,20,128);
	}
	OLED_Update();

}



void SystemSET_SysClockConfig_Max()
{
	SystemInit();
	menuOptions.SystemClock=72;
	Pointer.ReversePos=0;
}


void SystemSET_SysClockConfig_Medium()
{
	SystemSET_SysClockConfig_HSI();
	uint32_t t = 0;
	/*
	// 检查 HSI 是否已经启用
    if (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET)
    {
        RCC_HSICmd(ENABLE);
        
        while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET && ++t < 0x2000)
        {
            if (t >= 0x2000) 
            {
                break;
            }
        }
    }
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);    
	SystemCoreClock = 8000000;
	SystemCoreClockUpdate();	

	*/
	RCC_HSEConfig(RCC_HSE_ON);  
	t = 0;
    while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET && t++ < 0x2000)
    {
        if (t >= 0x2000) 
        {
            // HSE 启动失败，可返回错误或保持 HSI
            return;
        }
    }
	
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_5);
	t = 0;
	FLASH_SetLatency(FLASH_Latency_2);
	RCC_PLLCmd(ENABLE);
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET && ++t < 0x8000)
	{
		if (t >= 0x8000) 
		{
			break;
		}
	}
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	
	t = 0;
	while (++t < 0x1000)
	{
		if (t >= 0x1000) 
		{
			break;
		}
	}
	RCC_HSICmd(DISABLE);
	menuOptions.SystemClock=40;
	SystemCoreClock = 40000000;
	SystemCoreClockUpdate();
	Pointer.ReversePos=1;
	
	
}


void SystemSET_SysClockConfig_Low()
{
	uint32_t t = 0;
	// 检查 HSI 是否已经启用
    if (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET)
    {
        RCC_HSICmd(ENABLE);
        
        while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET && ++t < 0x2000)
        {
            if (t >= 0x2000) 
            {
                break;
            }
        }
    }
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);    
	SystemCoreClock = 8000000;
	SystemCoreClockUpdate();	

	SystemSET_SysClockConfig_HSI();
	
	RCC_HSEConfig(RCC_HSE_ON);  
	t = 0;
    while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET && t++ < 0x4000)
    {
        if (t >= 0x4000) 
        {
            // HSE 启动失败，可返回错误或保持 HSI
            return;
        }
    }
	
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_3);
	t = 0;
	FLASH_SetLatency(FLASH_Latency_2);
	RCC_PLLCmd(ENABLE);
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET && ++t < 0x8000)
	{
		if (t >= 0x8000) 
		{
			break;
		}
	}
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	
	t = 0;
	while (++t < 0x1000)
	{
		if (t >= 0x1000) 
		{
			break;
		}
	}
	RCC_HSICmd(DISABLE);
	menuOptions.SystemClock=24;
	SystemCoreClock = 24000000;
	SystemCoreClockUpdate();
	Pointer.ReversePos=2;
	
		
}

void SystemSET_SysClockConfig_HSI()
{
	RCC_HSICmd(ENABLE);
	uint32_t t = 0;
	while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET && ++t < 0x3000)
	{
		if (t >= 0x3000) 
		{
			break;
		}
	}
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);    
	SystemCoreClock = 8000000;
	SystemCoreClockUpdate();	

	menuOptions.SystemClock=8;
	RCC_PLLCmd(DISABLE);
	RCC_HSEConfig(RCC_HSE_OFF); 
	Pointer.ReversePos=3;

}

void Display_Reverse()
{
	if(OledOptions.DisplayReverse)
	{
		OledOptions.DisplayReverse=0;
	}
	else OledOptions.DisplayReverse=1;
	SystemSET_ShowCurPage();
}

void Display_Note()
{
	OLED_ClearBuf();
	OLED_FillRect(0,0,63,128,0);
	OLED_FillRect(1,1,62,126,1);
	OLED_AutoWrappingString(2, 2, 16, 8, "Search for CTRL66666 on GitHub for more support", 120, 0);
	OLED_Update();
}