/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"

/** @addtogroup STM32F10x_StdPeriph_Template
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.“自动重装载” 定时器
  * @param  None
  * @retval None
  */
volatile uint16_t PressDuration=0;	//定义按压时长静态变量,用于处理按键逻辑判断

//加中间变量缓存该事件
void SysTick_Handler(void)	//传事件,通过事件确定检测哪个按键的电平变化,不断累计按下时长直至松手,根据最后时长确定入队相应事件的点击或长按事件
{
	#include "EventBus.h"
	#include "Key.h"
	static EventType	eventTemp;
	eventTemp=CurrentEvent;
	u8 Key_Pressed=Is_KeyPressed(eventTemp);	//CurrentEvent是eventtype枚举类型,CurrentEvent+1是单击枚举,+2是长按枚举符号
	if (Key_Pressed) 
	{
        /* 计时逻辑 */
        if (PressDuration < 0xFFFF) PressDuration++;
        return;                      // 啥也不干，等下一次 10 ms
    }

    /* 2. 松手了——一次过判定并发送事件 */
    SysTick->CTRL = 0;               // 先关掉计时器和中断，防止重入
    

    /* 根据 CurrentEvent 决定发哪个键的 click/long */
    EventType ev_click = EV_NONE;
    EventType ev_long  = EV_NONE;
	
    switch (eventTemp) 
	{
		case EV_KEY_BACK_RAW:
			ev_click = EV_KEY_BACK_CLICK;
			ev_long  = EV_KEY_BACK_LONGPRESS;
			break;
		case EV_KEY_OK_RAW:
			ev_click = EV_KEY_OK_CLICK;
			ev_long  = EV_KEY_OK_LONGPRESS;
			break;
		default:                         // 保险，不会进
			break;
    }

    /* 3. 按时长入队对应事件 */
    if (ev_click != EV_NONE) {
        if (PressDuration < 3) 
		{         	
			/* 判定干扰 */
            
        } 
		else if (PressDuration <100) 
		{  // 30~100 ms 算单击
			
            EventQueue_Enqueue(ev_click);
        } 
		else 
		{                          
			
            EventQueue_Enqueue(ev_long);
        }
    }
	else
	{
		//无事件处理
	}

    /* 4. 清静态变量，退出中断 */
    PressDuration = 0;
	//待确定好是干扰,单击或者长按逻辑后,直接EventQueue_Enqueue(EventType ev)加入事件到消息队列中,后在主循环处理
	
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
