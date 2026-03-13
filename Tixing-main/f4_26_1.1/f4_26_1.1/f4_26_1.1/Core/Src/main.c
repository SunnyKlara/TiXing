/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "xuanniu.h"
#include "lcd.h"
#include "ws2812.h"
#include "w25q128.h"
#include "vs1003.h"
#include "engine_audio.h"
#include "logo.h"  // 🆕 Logo上传模块
#include "stdio.h"
#include "rx.h"
#include "string.h"

// 外部变量声明
extern u8 ui;
extern u8 chu;
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* ARM Compiler 6 compatible printf redirect */
#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
__asm(".global __use_no_semihosting");
#else
#pragma import(__use_no_semihosting)
struct __FILE
{
  int handle;
};
#endif

FILE __stdout;

void _sys_exit(int x)
{
  x = x;
}

int fputc(int ch, FILE *f) 
{
  HAL_UART_Transmit(&huart2,(uint8_t *)&ch,1,50);
  return ch;
}
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
uint8_t WriteBuffer[100] = {0x01,0x02};
uint8_t ReadBuffer[100];

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Startup_TaillightFlash(void);  // 🏎️ 开机尾灯双闪动画
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  MX_TIM3_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  MX_UART5_Init();
  MX_USART2_UART_Init();
  MX_SPI3_Init();
  /* USER CODE BEGIN 2 */
  
  // ════════════════════════════════════════════════════════════════
  // 🏎️ 开机启动序列 - 模拟真实跑车启动仪式
  // ════════════════════════════════════════════════════════════════
  
  // 1️⃣ 初始化LCD并显示开机Logo
  LCD_Init();
  
  // 2️⃣ 初始化编码器
  Encoder_Init();
  
  // 3️⃣ 初始化PWM
  HAL_TIM_PWM_Start_IT(&htim3,TIM_CHANNEL_1);
  HAL_TIM_PWM_Start_IT(&htim3,TIM_CHANNEL_4);
  
  // 4️⃣ 初始化Flash存储
  W25Q128_Init();
  W25Q128_BufferRead(ReadBuffer,0, sizeof(ReadBuffer));
  if(ReadBuffer[1]!=0x02)
  {
	  W25Q128_EraseSector(0);
	  W25Q128_BufferWrite(WriteBuffer, 0, sizeof(WriteBuffer));
  }
  else
  {
	  deng_init();
  }
  
  // 5️⃣ 初始化VS1003音频芯片
  VS1003_Init();
  HAL_Delay(50);
  VS1003_SetVolumePercent(100);  // 🔊 最大音量！
  
  // 6️⃣ 初始化发动机音效模块
  EngineAudio_Init();
  
  // 7️⃣ 初始化WS2812 LED
  WS2812_Init();
  
  // ════════════════════════════════════════════════════════════════
  // 🏎️ 同步启动：Logo + 双闪 + 引擎声 同时开始！
  // ════════════════════════════════════════════════════════════════
  LCD_ui0();  // 显示开机Logo
  printf("ENGINE_START\n");  // 通知APP
  Startup_TaillightFlash();  // 双闪 + 引擎声（内部会处理同步）
  
  // 启动完成后点亮用户设置的灯光
  deng();
  
  // 设置默认界面
  ui = 5;
  chu = 5;
  menu_selected = 1;

  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_5,GPIO_PIN_RESET);
  HAL_UART_Receive_IT(&huart2,&rx_data,1);
  
  // 初始化Logo模块
  Logo_Init();
  
  // Sine wave test - commented out to avoid startup beep
  // VS1003_SineTest(0);
  // HAL_Delay(5000);
  // VS1003_SineTestOff();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
//	  VS1003_SineTest(1);
	  // 🔥 优化：Logo上传期间跳过LCD/Encoder/PWM刷新，提升10倍速度
	  if(Logo_GetState() != LOGO_STATE_RECEIVING) {
		  LCD();
		  Encoder();
		  PWM();
		  LED_GradientProcess();  // ✅ LED平滑渐变处理（50fps）
		  Streamlight_Process();  // ✅ 流水灯效果处理
		  Breath_Process();       // ✅ 呼吸灯效果处理（UI4外持续运行）
	  }
	  RX_proc();  // 🔵 蓝牙命令接收处理
	  Logo_ProcessBuffer();  // 🔵 处理Logo缓冲区 (从缓冲区写入Flash)
	  EngineAudio_Process();  // 🔊 音频播放处理（每次循环都调用，确保数据持续发送）

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief  开机尾灯双闪动画 - 跑车启动仪式感
 * @note   配合5秒启动音效的两次轰鸣（4-9秒片段）：
 *         - 第一次轰鸣 (~1.0s): 第一次闪烁
 *         - 第二次轰鸣 (~3.0s): 第二次闪烁
 *         - 然后渐亮到深红色
 */
void Startup_TaillightFlash(void) {
    extern uint8_t bright;
    
    // 🔊 立即播放启动音效（最大音量）
    VS1003_SetVolumePercent(100);
    EngineAudio_PlayStart();
    
    // ════════════════════════════════════════════════════════════
    // 🏎️ 双闪配合两次轰鸣（总时长约5秒）
    // 音频是原始4-9秒片段，两次轰鸣大约在1秒和3秒位置
    // ════════════════════════════════════════════════════════════
    
    // 0-800ms: 预热（微弱橙光）
    WS2812B_SetAllLEDs(4, 40, 10, 0);
    WS2812B_Update(4);
    for(int j = 0; j < 80; j++) { EngineAudio_Process(); HAL_Delay(10); }
    
    // 800-1800ms: 第一次轰鸣 - 强闪！
    WS2812B_SetAllLEDs(4, 255, 80, 0);  // 亮橙红
    WS2812B_Update(4);
    for(int j = 0; j < 80; j++) { EngineAudio_Process(); HAL_Delay(10); }  // 800ms亮
    
    WS2812B_SetAllLEDs(4, 30, 8, 0);  // 微弱
    WS2812B_Update(4);
    for(int j = 0; j < 100; j++) { EngineAudio_Process(); HAL_Delay(10); }  // 1000ms暗
    
    // 2800-3600ms: 第二次轰鸣 - 强闪！
    WS2812B_SetAllLEDs(4, 255, 80, 0);  // 亮橙红
    WS2812B_Update(4);
    for(int j = 0; j < 80; j++) { EngineAudio_Process(); HAL_Delay(10); }  // 800ms亮
    
    WS2812B_SetAllLEDs(4, 30, 8, 0);  // 微弱
    WS2812B_Update(4);
    for(int j = 0; j < 60; j++) { EngineAudio_Process(); HAL_Delay(10); }  // 600ms暗
    
    // 4200-5000ms: 渐亮到深红色
    for(int step = 0; step < 16; step++) {
        uint8_t r = 30 + step * 10;  // 30 -> 190
        uint8_t g = 8 - step / 2;    // 8 -> 0
        if(g > 200) g = 0;
        WS2812B_SetAllLEDs(4, r, g, 0);
        WS2812B_Update(4);
        for(int j = 0; j < 5; j++) { EngineAudio_Process(); HAL_Delay(10); }
    }
    
    // 最终状态: 深红色尾灯常亮
    WS2812B_SetAllLEDs(4, 
        (uint8_t)(180 * bright * bright_num),
        0,
        0);
    WS2812B_Update(4);
    
    // 发送引擎就绪通知
    printf("ENGINE_READY\n");
    
    // 等待启动音效播放完毕
    while(EngineAudio_IsPlaying()) {
        EngineAudio_Process();
        HAL_Delay(10);
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
