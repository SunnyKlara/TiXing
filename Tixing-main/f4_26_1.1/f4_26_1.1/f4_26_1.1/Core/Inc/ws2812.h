/* ws2812b_bitbang.h - 基于GPIO位操作的WS2812驱动头文件 */
#ifndef __WS2812B_H
#define __WS2812B_H

#include "main.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "lcd_init.h"

/* 定义LED数量和颜色数据结构 */
#define LED1_COUNT 30
#define LED2_COUNT 30

typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} WS2812_ColorTypeDef;

/* 函数声明 */
void WS2812_Init(void);
void WS2812_SetLED1Color(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void WS2812_SetLED2Color(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void WS2812_ShowLED1(void);
void WS2812_ShowLED2(void);
void WS2812_SetAllLED1Color(uint8_t r, uint8_t g, uint8_t b);
void WS2812_SetAllLED2Color(uint8_t r, uint8_t g, uint8_t b);
void WS2812_SetLED1RangeColor(uint16_t start, uint16_t count, uint8_t r, uint8_t g, uint8_t b);
void WS2812_SetLED2RangeColor(uint16_t start, uint16_t count, uint8_t r, uint8_t g, uint8_t b);
void WS2812B_SetAllLEDs(uint8_t strip_num, uint8_t r, uint8_t g, uint8_t b);
void WS2812B_Set23LEDs(uint8_t strip_num, uint8_t r, uint8_t g, uint8_t b);
void WS2812B_Update(uint8_t strip_num);
#endif
