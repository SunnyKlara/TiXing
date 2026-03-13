/* ---------------- Encoder.c ---------------- */
/* ---------------- 编码器初始化和中断配置 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include "OLED.h"
#include "Delay.h"
#include "Key.h"
uint16_t Encoder_Count;
uint8_t Speed;

/**
  * @brief  配置定时器编码器模式
  * @param  无
  * @retval 无
  */
void Encoder_Init(void)
{
	//总线1使能通用定时器3时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);//总线2使能PA0
	
	
	//GPIO引脚配置
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IPU;//上拉输入
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure); 
	
	//现在不配置定时器的内部时钟,而编码器接口就是带计数方向的外部时钟,
	
	//初始化时基单元      
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1;//一分频;在编码器模式里根本无效
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up;//向上计数;在编码器模式里根本无效
	TIM_TimeBaseInitStructure.TIM_Period = 65535-1; //AAR自动重装器的值
	TIM_TimeBaseInitStructure.TIM_Prescaler =1 -1;//PSC预分频器的值;在编码器模式里根本无效
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter	=0;//重复计数器,高级计数器才有
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseInitStructure);//时基初始化函数中,为了让设置预分频器的值立刻生效,立刻生成一个更新事件,产生更新事件标志位,并进入一次中断
	
	
	//若未调用TIM_EncoderInterfaceConfig()，定时器仍以默认的时钟驱动模式运行
	TIM_EncoderInterfaceConfig(TIM3,TIM_EncoderMode_TI12,TIM_ICPolarity_Rising,TIM_ICPolarity_Rising);//在双边沿均计数

	/* ---------------- 计数器初始值 ---------------- */
	uint16_t start = 100;//初始值
    TIM_SetCounter(TIM3, start);

	 /* ---------------- CCR1：正转触发 ---------------- */
	// 配置捕获/比较通道1
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCMode=TIM_OCMode_Timing;//冻结模式，匹配时不改变输出电平，仅触发中断或DMA
	TIM_OCInitStructure.TIM_OCPolarity=TIM_OCPolarity_High;//高电平为有效极性
	TIM_OCInitStructure.TIM_OutputState=TIM_OutputState_Disable;		//不输出
	TIM_OCInitStructure.TIM_Pulse=104;		//		CCR捕获比较寄存器的值,用于匹配计数值,匹配成功进中断
	TIM_OC1Init(TIM3,&TIM_OCInitStructure);//定时器3的OC通道1
	
	 /* ---------------- CCR2：反转触发 ---------------- */
	
	TIM_OCInitStructure.TIM_Pulse=96;		//		CCR捕获比较寄存器的值,用于匹配计数值,匹配成功进中断
	TIM_OC2Init(TIM3,&TIM_OCInitStructure);//定时器3的OC通道2
	
	
	/*----------------启用更新事件CC1/CC2 中断----------------*/
	
    TIM_ITConfig(TIM3, TIM_IT_CC1 | TIM_IT_CC2, ENABLE);//配置使能比较捕获CCR匹配事件中断,启用定时器3的捕获/比较通道的中断
	
	/*----------------配置定时器3中断优先级----------------*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//2抢占级2优先级
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel= TIM3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=2;
	
	NVIC_Init(&NVIC_InitStructure);
	
	EventBus_Subscribe(EV_ENC_CW, Left_Shift);	//订阅默认函数:当逆旋键判定时
	EventBus_Subscribe(EV_ENC_CCW, Right_Shift);	//订阅默认函数:当顺旋判定时
	
	/*----------------启动定时器----------------*/
	TIM_Cmd(TIM3,ENABLE);
}



/**
  * @brief  定时器3中断处理函数,处理编码器事件
  * @param  无
  * @retval 无
  */
void TIM3_IRQHandler(void)
{


	if (TIM_GetITStatus(TIM3, TIM_IT_CC1) != RESET)   // 逆时针转
	{

		EventQueue_Enqueue(EV_ENC_CW);
		
		TIM_SetCounter(TIM3, 100);//恢复默认值
		TIM_ClearITPendingBit(TIM3, TIM_IT_CC1);
	}

	if (TIM_GetITStatus(TIM3, TIM_IT_CC2) != RESET)   // 顺时针转
	{

		EventQueue_Enqueue(EV_ENC_CCW);
		
		//编码器恢复默认值
		TIM_SetCounter(TIM3, 100);
		TIM_ClearITPendingBit(TIM3, TIM_IT_CC2);
	}
	
	Encoder_Count++;
	
}
	



/*
	TIM_EncoderInterfaceConfig()函数通过设置定时器的SMCR（从模式控制寄存器）和CCER（捕获/比较使能寄存器），告知硬件进入编码器接口模式：
	SMCR.SMS[2:0]：设置为011时启用编码器模式（如TIM_EncoderMode_TI12）。
	CCER.CC1P/CC2P：设置输入信号极性（是否反相）
	一、函数配置的意义
		编码器模式选择（TIM_EncoderMode_TI12）

		作用：使定时器在TI1（A相）和TI2（B相）的边沿均计数，实现四倍频（每个信号周期触发4次计数）。
		原理：当编码器旋转时，A相和B相的相位差为90°（正交信号）。若仅在单边沿计数（如TIM_EncoderMode_TI1或TIM_EncoderMode_TI2），每个周期计数值变化2次；若在双边沿均计数（TI12模式），计数值变化4次，分辨率提高一倍310。
		输入极性设置（TIM_ICPolarity_Rising）

		作用：设置编码器信号的有效边沿极性。Rising表示捕获上升沿，但实际在编码器模式下，此参数控制的是信号是否反相1217。
		原理：
		若A相和B相的极性均设为Rising（不反相），则当A相领先B相时，计数器递增；反之递减。
		若某一相设为Falling，会将该相信号反相，从而改变方向判断逻辑（例如硬件引脚调换时可通过软件修正方向）
*/