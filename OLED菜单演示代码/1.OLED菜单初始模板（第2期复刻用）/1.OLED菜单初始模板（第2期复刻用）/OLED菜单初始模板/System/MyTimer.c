/* ---------------- MyTimer.c ---------------- */
/* ---------------- 定时器模块,定时刷新屏幕和发布事件 ---------------- */
#include "stm32f10x.h"
#include "Encoder.h"
#include "Image.h"
#include "Key.h"
#include "OLED.h"
#include "EventBus.h"
#include "MyTimer.h"
TimerTask TaskTimer=
{
	.UpdateTask=0,
};

void Timer_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
	//配置内部时钟
	TIM_InternalClockConfig(TIM2);//默认即使用内部时钟,不写也行
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;//一分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;//向上计数.此外还有向下计数和中央对其共三种模式
	TIM_TimeBaseInitStructure.TIM_Period = 10000-1; //自动重装器的值
	TIM_TimeBaseInitStructure.TIM_Prescaler =720 -1;//PSC预分频器的值
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter	=0;//重复计数器,高级计数器才有
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStructure);//时基初始化函数中,为了让设置预分频器的值立刻生效,立刻生成一个更新事件,产生更新事件标志位,并进入一次中断
	
	TIM_ClearFlag(TIM2,TIM_FLAG_Update);//清除更新事件标志位
	TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE);//配置使能更新事件中断;
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//2抢占级2优先级
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel= TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=3;
	NVIC_Init(&NVIC_InitStructure);
	
	EventBus_Subscribe(EV_TIMER_OLED_UPDATE, OLED_Update);
	
	TIM_Cmd(TIM2,ENABLE);//启动定时器
}


uint8_t FPS;
static u8 Count=0;
/**
  * @brief  定时器分配定时刷写任务或动作钩函数
  */
void TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM2,TIM_IT_Update)==SET)//获取TIM2的更新标志位
	{
		
		if(EventBus_IsVoid(EV_TIMER_TASK))
		{
			EventBus_Publish(EV_TIMER_TASK);	//此处若入队处理,有黑屏产生
		}

		if(TaskTimer.UpdateTask)	//update刷新任务
		{
			EventBus_Publish(EV_TIMER_OLED_UPDATE);
		}
		
	
	
		
		//帧速率监测
		if (Count>=10)
		{
			//Encoder_Get();
			FPS=FPS_Get();
		}
		//计数判断
		if (Count>=10) Count=0;
		Count++;;
		//清除标志位
		TIM_ClearITPendingBit(TIM2,TIM_IT_Update);
	}
}
//设置定时器分频系数以配置定时器中断周期
void Timer_SetPrescaler(uint16_t prescaler)
{
    TIM_PrescalerConfig(TIM2, prescaler - 1, TIM_PSCReloadMode_Immediate);
}
