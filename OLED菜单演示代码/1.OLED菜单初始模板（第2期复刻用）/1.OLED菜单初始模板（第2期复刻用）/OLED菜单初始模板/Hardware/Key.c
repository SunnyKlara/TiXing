/* ---------------- Key.c ---------------- */
/* ---------------- 按键初始化和中断配置,通过systick系统中断消抖,并判断短按,长按等事件再投递对应事件 ---------------- */
/* ---------------- 采用观察者模式和消息队列,订阅和投递事件并处理 ---------------- */
#include "stm32f10x.h"       
#include "Delay.h"
#include "Image.h"
#include "OLED.h"
#include "EventBus.h"
#include "Key.h"

void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;//上拉输入,按下低电平,松手高电平
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0 | GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitStructure); 
	
	// 映射 PB0(OK) 到 EXTI0
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);

	// 映射 PB11(BACK) 到 EXTI11
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource11);
	
	EXTI_InitTypeDef EXTI_InitStruct;

	
	// EXTI0（PB0）
	EXTI_InitStruct.EXTI_Line = EXTI_Line0;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;//当触发条件满足（如下降沿），会跳转到对应的中断服务函数（ISR）执行代码。
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling; // 下降沿触发
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	// EXTI11（PB11）
	EXTI_InitStruct.EXTI_Line = EXTI_Line11;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	
	NVIC_InitTypeDef NVIC_InitStruct;
	
	//中断优先级配置和使能
	// EXTI0（PB0）
	NVIC_InitStruct.NVIC_IRQChannel = EXTI0_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);

	// EXTI15_10（PB11）
	NVIC_InitStruct.NVIC_IRQChannel = EXTI15_10_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStruct);
	/* ---------------- 观察者默认订阅操作 ---------------- */
	
	EventBus_Subscribe(EV_KEY_OK_CLICK, MenuForward);	//订阅默认函数:当OK键判定单击时
	EventBus_Subscribe(EV_KEY_BACK_CLICK, MenuBack);	//订阅默认函数:当BACK键判定单击时
	EventBus_Subscribe(EV_KEY_BACK_RAW, KeyLogic_Process_Back);	//订阅默认函数:进入systick判定消抖逻辑
	EventBus_Subscribe(EV_KEY_OK_RAW,KeyLogic_Process_OK);
	
}




void KeyLogic_Process(EventType ev)
{
	SysTick_Config(SystemCoreClock / 100);	//10ms进入系统中断
	SysTick->VAL = 0x00;					//清空当前计数值
	SysTick->CTRL = SysTick_CTRL_ENABLE_Msk |       // ① 启动
                SysTick_CTRL_TICKINT_Msk   |     // ② 允许中断
                SysTick_CTRL_CLKSOURCE_Msk;     // ③ 时钟源=HCLK
	CurrentEvent=ev;	//当前处理事件指向ev
}

void KeyLogic_Process_Back(void)
{
	KeyLogic_Process(EV_KEY_BACK_RAW);
}

void KeyLogic_Process_OK(void)
{
	KeyLogic_Process(EV_KEY_OK_RAW);

}

//检测中断信号事件对应的按键是否仍被按下
uint8_t Is_KeyPressed(EventType ev)
{
	uint16_t 	Pin=0;	
	if(ev==EV_KEY_OK_RAW)			Pin=GPIO_Pin_0;
	else if(ev==EV_KEY_BACK_RAW)	Pin=GPIO_Pin_11;

	return GPIO_ReadInputDataBit(GPIOB, Pin) == Bit_RESET;//GPIO_PIN
	
}



// PB0 的中断处理函数
void EXTI0_IRQHandler(void)//挂起标志位（Pending Bit）为SET时跳转EXTIx_IRQHandler()
{
	
	
    if (EXTI_GetITStatus(EXTI_Line0) != RESET)	//
    {
		
		EventQueue_Enqueue(EV_KEY_OK_RAW);

    }
		
	EXTI_ClearITPendingBit(EXTI_Line0);
}

// PB11 的中断（在 EXTI15_10_IRQHandler 中）
void EXTI15_10_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line11) != RESET )	//&& dma_done不可行,容易吞操作
    {
		
		EventQueue_Enqueue(EV_KEY_BACK_RAW);	//

    }
	EXTI_ClearITPendingBit(EXTI_Line11);
}