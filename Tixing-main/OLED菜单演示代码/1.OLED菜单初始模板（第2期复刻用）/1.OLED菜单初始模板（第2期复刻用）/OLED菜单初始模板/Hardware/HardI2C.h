/* ---------------- HardI2C.h ---------------- */

#ifndef __HARDI2C_H
#define __HARDI2C_H

#define HARDI2C_ENABLED  // 硬件I2C 被启用
#define OLED_I2C_ADDR   (0x3C<<1)   // 0x78 的 7 位形式
#define CMD_CTRL_BYTE   0x00        // Co=0, D/C#=0
#define DAT_CTRL_BYTE   0x40        // Co=0, D/C#=1

/* ---------------- 全局变量 ---------------- */
extern volatile uint8_t dma_done;   // 1=完成，0=正在传
typedef enum { 
	OLED_SSD1306, 
	OLED_SH1106 
} OLED_Type;

extern OLED_Type g_oled_type;
extern volatile uint8_t dma_chain_page;      // 当前刷到第几页（SH1106）
extern volatile uint8_t dma_chain_busy;

/* ---------------- 接口函数 ---------------- */
void OLED_I2C_Init(void);
void OLED_WriteCommand(uint8_t Command);//用DMA写命令
void OLED_WriteData(uint8_t data);
void OLED_HardWriteCommand(uint8_t Command);//硬件I2C直接写命令
void SH1106_SendPage(uint8_t *p_buf, uint8_t page);
#endif