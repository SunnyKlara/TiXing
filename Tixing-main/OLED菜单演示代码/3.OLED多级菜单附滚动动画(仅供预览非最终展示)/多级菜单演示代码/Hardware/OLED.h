/* ---------------- OLED.h ---------------- */
#ifndef __OLED_H
#define __OLED_H

#define BUF_CNT 2
/* ---------------- 全局变量 ---------------- */
extern uint8_t OLED_BUF[BUF_CNT][8][128];   // 8×128×2,双缓冲数组
extern uint8_t ImageTemp[8*128];	//OLED临时显存
extern volatile uint8_t buf_idx;	//双缓冲显存索引

/*----------------菜单选项定义----------------*/
typedef struct OLEDOptions{
	volatile uint8_t DisplayReverse;//显示反色控制位

} OLEDOptions;
extern OLEDOptions OledOptions;
/* ---------------- 接口函数 ---------------- */
#define DRAW_BUF  OLED_BUF[buf_idx]         // 正在画
#define SEND_BUF  OLED_BUF[buf_idx^1]       // 正在发
void OLED_Init(void);			//OLED第一次使用时初始化
void OLED_Refresh(void);		//刷新屏幕,用于修复IIC通信导致的像素错位
void OLED_SetCursor(uint8_t Y, uint8_t X);		//主要用于软件IIC,硬件IIC无需此函数
void OLED_Clear(void);			//OLED直接清屏
void OLED_ClearBuf(void);		//OLED显存数组重置
void OLED_ClearBufArea(uint8_t Line, uint8_t Column,uint8_t Height,uint8_t Width);	//对OLED显存数组指定区域擦除为0
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char,...);		//以ShowImage函数为底座构建的字符函数
void OLED_UpdateArea(uint8_t Line, uint8_t Column,uint8_t Height,uint8_t Width);	//将缓存数组部分区域更新到显示屏上,与DMA硬件不兼容
void OLED_Update(void);			//一次性发送整个显存（1024字节）
void OLED_Update_old(void);		//旧算法更新显示屏,适用软件I2C
void OLED_OffsetUpdate(int8_t Offset_Line,int8_t Offset_Column);	//将整个缓存数组按指定参数偏移并更新到显示屏上,
void OLED_ShowImage(int8_t Line, int8_t Column,uint8_t Height,uint8_t Width, const uint8_t *Image,uint8_t Mode);
void OLED_DrawPixel(int8_t Line, int8_t Column);	//在指定坐标画一个点
void OLED_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t is_dashed);	
void OLED_FillRect(int8_t Line, int8_t Column, uint8_t Height, uint8_t Width,uint8_t Reverse);	//在OLED缓存数组内画一个实心矩形（填充）,以OLED_ShowImage为底层函数,
void OLED_ReverseArea(int8_t Line, int8_t Column, uint8_t Height, uint8_t Width);				//在指定位置按给定的长和宽形成的矩形区域反色显示
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String,uint8_t Reverse,...);		//OLED显示字符串
uint8_t OLED_ShowChinese(uint16_t Line, uint16_t Column, char *String,uint8_t Height,uint8_t Width,uint8_t Mode,uint8_t Reverse);
uint8_t OLED_AutoWrappingString(uint16_t Line, uint16_t Column, uint8_t Height, uint8_t Width, char *String, uint8_t lineLen, uint8_t Reverse);
void OLED_Printf(int8_t Line, uint8_t Column,uint8_t Height,uint8_t Width,uint8_t Reverse, char *fmt, ...);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length,uint8_t Height,uint8_t Width,uint8_t Reverse);//OLED显示数字（十进制，正数）
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled);
extern volatile uint8_t dma_done;	//DMA转运完毕标志
uint8_t FPS_Get();	//配合定时器获取刷写帧率
#endif
