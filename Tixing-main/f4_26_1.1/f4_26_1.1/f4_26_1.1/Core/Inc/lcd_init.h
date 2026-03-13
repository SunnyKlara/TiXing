#ifndef _lcd_init_H_
#define _lcd_init_H_

#include "main.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

#define USE_HORIZONTAL 2  //设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏


#define LCD_WIDTH 240
#define LCD_HEIGHT 240

void Delay_us(uint32_t us);
void LCD_Init(void);
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2);
void LCD_WR_REG(u8 dat);
void LCD_WR_DATA(u16 dat);
void LCD_WR_DATA8(u8 dat);
void LCD_Writ_Bus(uint8_t *data, uint16_t size);
void LCD_WR_DATA_BUF(uint16_t *data, uint32_t size);
#endif
