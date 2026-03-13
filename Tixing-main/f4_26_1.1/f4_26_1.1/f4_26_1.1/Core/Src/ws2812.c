/* ws2812b_bitbang.c - 基于GPIO位操作的WS2812驱动实现文件 */
#include "ws2812.h"
#include "stm32f4xx_hal.h"

/* 颜色数据缓冲区 */
static WS2812_ColorTypeDef LED1_ColorBuffer[LED1_COUNT];
static WS2812_ColorTypeDef LED2_ColorBuffer[LED2_COUNT];

/* DWT延时函数 - 基于CPU周期计数 */
static inline void delay_cycles(uint32_t cycles) {
    uint32_t start = DWT->CYCCNT;
    while((DWT->CYCCNT - start) < cycles);
}

/* 启用DWT计时器 */
static void DWT_Init(void) {
    /* 启用DWT访问 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    /* 启用CYCCNT计数器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    /* 清零计数器 */
    DWT->CYCCNT = 0;
}
//67,142
/* 发送逻辑1 - 850ns高电平 + 400ns低电平 */
static inline void send_bit_1_LED1(void) {
    GPIOC->BSRR = GPIO_PIN_11;      // 设置PC11为高电平 (替代HAL_GPIO_WritePin)
    delay_cycles(120);              // 850ns @168MHz
    GPIOC->BSRR = GPIO_PIN_11 << 16; // 设置PC11为低电平 (替代HAL_GPIO_WritePin)
    delay_cycles(35);               // 400ns @168MHz
}

static inline void send_bit_1_LED2(void) {
    GPIOA->BSRR = GPIO_PIN_12;      // 设置PA12为高电平
    delay_cycles(120);
    GPIOA->BSRR = GPIO_PIN_12 << 16; // 设置PA12为低电平
    delay_cycles(35);
}

/* 发送逻辑0 - 400ns高电平 + 850ns低电平 */
static inline void send_bit_0_LED1(void) {
    GPIOC->BSRR = GPIO_PIN_11;      // 设置PC11为高电平
    delay_cycles(35);               // 400ns @168MHz
    GPIOC->BSRR = GPIO_PIN_11 << 16; // 设置PC11为低电平
    delay_cycles(120);              // 850ns @168MHz
}

static inline void send_bit_0_LED2(void) {
    GPIOA->BSRR = GPIO_PIN_12;      // 设置PA12为高电平
    delay_cycles(35);
    GPIOA->BSRR = GPIO_PIN_12 << 16; // 设置PA12为低电平
    delay_cycles(120);
}

/* 发送一个字节数据 */
static void send_byte_LED1(uint8_t byte) {
    uint8_t i;
    for(i = 0; i < 8; i++) {
        if(byte & 0x80) {
            send_bit_1_LED1();
        } else {
            send_bit_0_LED1();
        }
        byte <<= 1;
    }
}

static void send_byte_LED2(uint8_t byte) {
    uint8_t i;
    for(i = 0; i < 8; i++) {
        if(byte & 0x80) {
            send_bit_1_LED2();
        } else {
            send_bit_0_LED2();
        }
        byte <<= 1;
    }
}

/* 初始化函数 */
void WS2812_Init(void) {
    /* 初始化DWT计时器 */
    DWT_Init();
    
    /* 初始化为黑色 */
    WS2812_SetAllLED1Color(0, 0, 0);
    WS2812_SetAllLED2Color(0, 0, 0);
    WS2812_ShowLED1();
    WS2812_ShowLED2();
}

/* 设置单个LED颜色 - 灯带1 */
void WS2812_SetLED1Color(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if(index < LED1_COUNT) {
        LED1_ColorBuffer[index].r = r;
        LED1_ColorBuffer[index].g = g;
        LED1_ColorBuffer[index].b = b;
    }
}

/* 设置单个LED颜色 - 灯带2 */
void WS2812_SetLED2Color(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if(index < LED2_COUNT) {
        LED2_ColorBuffer[index].r = r;
        LED2_ColorBuffer[index].g = g;
        LED2_ColorBuffer[index].b = b;
    }
}

/* 更新灯带1显示 */
void WS2812_ShowLED1(void) {
    uint16_t i;
    
    /* 关闭中断以确保时序精确 */
    __disable_irq();
    
    /* 发送所有LED数据 */
    for(i = 0; i < LED1_COUNT; i++) {
        /* 按GRB顺序发送数据 */
        send_byte_LED1(LED1_ColorBuffer[i].g);
        send_byte_LED1(LED1_ColorBuffer[i].r);
        send_byte_LED1(LED1_ColorBuffer[i].b);
    }
    
    /* 开启中断 */
    __enable_irq();
    
    /* 发送复位信号 - 至少50us低电平 */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_11, GPIO_PIN_RESET);
    delay_cycles(10000);  // 50us @168MHz (168*50=8400)
}

/* 更新灯带2显示 */
void WS2812_ShowLED2(void) {
    uint16_t i;
    
    /* 关闭中断以确保时序精确 */
    __disable_irq();
    
    /* 发送所有LED数据 */
    for(i = 0; i < LED2_COUNT; i++) {
        /* 按GRB顺序发送数据 */
        send_byte_LED2(LED2_ColorBuffer[i].g);
        send_byte_LED2(LED2_ColorBuffer[i].r);
        send_byte_LED2(LED2_ColorBuffer[i].b);
    }
    
    /* 开启中断 */
    __enable_irq();
    
    /* 发送复位信号 - 至少50us低电平 */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    delay_cycles(10000);  // 50us @168MHz
}

/* 设置所有LED颜色 - 灯带1 */
void WS2812_SetAllLED1Color(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t i;
    for(i = 0; i < LED1_COUNT; i++) {
        WS2812_SetLED1Color(i, r, g, b);
    }
}

/* 设置所有LED颜色 - 灯带2 */
void WS2812_SetAllLED2Color(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t i;
    for(i = 0; i < LED2_COUNT; i++) {
        WS2812_SetLED2Color(i, r, g, b);
    }
}
/* ws2812b_direct.c - 新增函数 */

/**
 * @brief 设置灯带1中指定范围的LED颜色
 * @param start: 起始LED索引(从0开始)
 * @param count: 需要设置的LED数量
 * @param r: 红色值(0-255)
 * @param g: 绿色值(0-255)
 * @param b: 蓝色值(0-255)
 */
void WS2812_SetLED1RangeColor(uint16_t start, uint16_t count, uint8_t r, uint8_t g, uint8_t b) {
    uint16_t i, end;
    
    /* 计算结束索引 */
    end = start + count;
    if(end > LED1_COUNT) end = LED1_COUNT;
    
    /* 设置范围内的LED颜色 */
    for(i = start; i < end; i++) {
        WS2812_SetLED1Color(i, r, g, b);
    }
}

/**
 * @brief 设置灯带2中指定范围的LED颜色
 * @param start: 起始LED索引(从0开始)
 * @param count: 需要设置的LED数量
 * @param r: 红色值(0-255)
 * @param g: 绿色值(0-255)
 * @param b: 蓝色值(0-255)
 */
void WS2812_SetLED2RangeColor(uint16_t start, uint16_t count, uint8_t r, uint8_t g, uint8_t b) {
    uint16_t i, end;
    
    /* 计算结束索引 */
    end = start + count;
    if(end > LED2_COUNT) end = LED2_COUNT;
    
    /* 设置范围内的LED颜色 */
    for(i = start; i < end; i++) {
        WS2812_SetLED2Color(i, r, g, b);
    }
}
void WS2812B_SetAllLEDs(uint8_t strip_num, uint8_t r, uint8_t g, uint8_t b) 
{
	if(strip_num == 2)
	{
		WS2812_SetLED1RangeColor(0,3,r,g,b);
	}
	else if(strip_num == 3)
	{
		WS2812_SetLED1RangeColor(9,3,r,g,b);
	}
	else if(strip_num == 4)
	{
		WS2812_SetLED2RangeColor(0,3,r,g,b);
	}
	
	else if(strip_num == 1)
	{
		WS2812_SetLED1RangeColor(3,6,r,g,b);
	}
}
void WS2812B_Set23LEDs(uint8_t strip_num, uint8_t r, uint8_t g, uint8_t b)
{
	if(strip_num == 2)
	{
		WS2812_SetLED1RangeColor(0,6,r,g,b);
	}
	else if(strip_num == 3)
	{
		WS2812_SetLED1RangeColor(6,6,r,g,b);
	}
}
void WS2812B_Update(uint8_t strip_num)
{
	if(strip_num == 2 || strip_num == 3 || strip_num == 1)
	{
		WS2812_ShowLED1();
	}
	else if(strip_num == 4)
	{
		WS2812_ShowLED2();
	}
}
