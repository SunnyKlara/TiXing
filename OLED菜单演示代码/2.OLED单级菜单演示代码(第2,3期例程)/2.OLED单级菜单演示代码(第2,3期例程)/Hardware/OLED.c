/* ---------------- OLED.c ---------------- */
/* ---------------- 基于HardI2C的OLED画图和显示方法,为驱动层 ---------------- */
/* ---------------- GND->PB4;VCC->PB5;SCL->PB6;SDA->PB7 ---------------- */
#include "stm32f10x.h"
#include "OLED_Font.h"
#include "OLED.h"
#include <string.h>
#include "HardI2C.h"
#include <stdarg.h>
#include "Image.h"
#include "Delay.h"
#include "math.h"
//#define SoftI2C_ENABLED	//启用软件I2C时,定义该项

/* ---------------- 显存配置区 ---------------- */
#define BUF_CNT 2
uint8_t OLED_BUF[BUF_CNT][8][128];
uint8_t ImageTemp[8*128];//必须大于最大字模数
volatile uint8_t buf_idx = 0;                 // 0/1 索引
#define DRAW_BUF  OLED_BUF[buf_idx]         // 正在画
#define SEND_BUF  OLED_BUF[buf_idx^1]       // 正在发

u8 Update_Count;	//计算帧率

/* ---------------- 变量定义 ---------------- */
OLEDOptions OledOptions=
{
	.DisplayReverse=0,//显示反色控制位

};

/* ---------------- 以下为软件模拟协议调用 ---------------- */
#ifndef	HARDI2C_ENABLED		//若不采用硬件DMA做OLED驱动,则采用以下方法
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_6, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_7, (BitAction)(x))

/*引脚初始化*/
void OLED_I2C_Init(void)	//软件I2C模拟初始化
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C开始
  */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}

/**
  * @brief  I2C停止
  */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
  * @brief  I2C发送一个字节
  * @param  Byte 要发送的一个字节
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(!!(Byte & (0x80 >> i)));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	OLED_W_SCL(1);	//额外的一个时钟，不处理应答信号
	OLED_W_SCL(0);
}


/**
  * @brief  OLED写命令
  * @param  Command 要写入的命令
  */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x00);		//写命令
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

/**
  * @brief  OLED写数据
  * @param  Data 要写入的数据
  */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		//从机地址
	OLED_I2C_SendByte(0x40);		//写数据
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}
/**
  * @brief  OLED写多字节数据
  * @param  len 长度
  */

void OLED_WriteDataBuf(const uint8_t *buf, uint16_t len)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);   // SA0=0, 写模式
    OLED_I2C_SendByte(0x40);   // 连续数据模式
    for (uint16_t i = 0; i < len; ++i)
        OLED_I2C_SendByte(buf[i]);
    OLED_I2C_Stop();
}

#endif

/* ---------------- 以下为硬件I2C+DMA通信调用 ---------------- */
/**
  * @brief  SSD136的OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~7
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_HardWriteCommand(0xB0 | Y);					//设置Y位置
	OLED_HardWriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位
	OLED_HardWriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位
	
	/*
	uint8_t cmd[4] = {0x00, 0xB0 + Y,0x10 | ((X & 0xF0) >> 4), 0x00 | (X & 0x0F)};
    
    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));
    
    I2C_Send7bitAddress(I2C1, OLED_I2C_ADDR, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
    

	
    for (int i = 0; i < 4; i++) {
        I2C_SendData(I2C1, cmd[i]);
        while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    }
    
    I2C_GenerateSTOP(I2C1, ENABLE);
	*/
}

/**
  * @brief  OLED清屏
  */
void OLED_Clear(void)
{  
	uint8_t i, j;
	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);
		for(i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
}
/**
  * @brief  缓存数组重置
  */
void OLED_ClearBuf(void)
{
	memset(DRAW_BUF, 0, sizeof(DRAW_BUF));//比双重for循环填0更减少性能开销
}
/**
* @brief  缓存数组指定矩形区域擦除数据,用来下次显示动作前将上次渲染区域的数据擦除
* @param  最小以页为单位擦除行
*/
void OLED_ClearBufArea(uint8_t Line, uint8_t Column,uint8_t Height,uint8_t Width)
{
	uint8_t i,j;
	u8 page_start=Line/8;
	u8 page_end	=(Line+Height)/8;
	u8 MaxWidth=Column+Width>127?128-Column:Width;//图像右侧边界处理
	if (MaxWidth > Width) MaxWidth=0;///右侧边界处理.须满足条件MaxWidth > Width
	if (page_start==page_end)//如果只有一页
	{
		for (i = 0;i < MaxWidth; i++)
		{
			DRAW_BUF[page_start][Column  + i] &= ~(0xFF<<Line & 0xFF >>(8-(Line+Height)));
		}	
	}
	else//多页的情形
	{
		for (i = 0;i < MaxWidth; i++)//先处理头页和尾页
		{
			DRAW_BUF[page_start][Column  + i] &= ~(0xFF << (Line % 8));			//首页数据清空覆盖
			DRAW_BUF[page_end][Column  + i] &= ~(0xFF >> (8 - ((Line+Height) % 8)));  //尾页数据清空覆盖
		}
		//处理中间擦除页
		uint8_t *p;
		for (uint8_t page = page_start+1;page < page_end; page++)
		{
			p = &DRAW_BUF[page][Column];
			memset(p, 0, MaxWidth);//把这一页、指定横向范围一次性全黑
		}
	}
}


// 一次性发送整个显存（1024字节）
void OLED_Update(void)
{
	// 1. 等待上一次传输完成（如果需要同步）
	uint32_t timeout = 25000;
	while (!dma_done)	//超时退出
	{
		
		timeout--;;
		if (timeout < 10) 
		{
		// 总线卡死，尝试恢复
		   
            DMA_Cmd(DMA1_Channel6, DISABLE);
			I2C_GenerateSTOP(I2C1, ENABLE);
            I2C_DMACmd(I2C1, DISABLE);
            dma_done = 1;
			DMA_ClearFlag(DMA1_FLAG_TC6 | DMA1_FLAG_TE6);	//清除指定的DMA中断标志,防止重复处理同一个中断
            //return;	//不能返回?!
		}
	};

	uint16_t total_bytes = 1024; // 1024总大小
	//
	if(OledOptions.DisplayReverse)	OLED_ReverseArea(0, 0, 64, 128);	//反色显示
	buf_idx ^= 1;//翻转
    uint8_t *p_data = (uint8_t*)SEND_BUF; // 指向显存起始地址
	
    // 3. 根据芯片类型选择策略
    if(g_oled_type == OLED_SSD1306) {
        // ===== SSD1306：一次 DMA 发 1024 B =====
        DMA1_Channel6->CMAR = (uint32_t)p_data;
        DMA1_Channel6->CNDTR = 1024;            // 整屏
        dma_done = 0;
        I2C_DMACmd(I2C1, ENABLE);
        I2C_GenerateSTART(I2C1, ENABLE);
        while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));
        I2C_Send7bitAddress(I2C1, OLED_I2C_ADDR, I2C_Direction_Transmitter);
        while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
        I2C_SendData(I2C1, DAT_CTRL_BYTE);      // 0x40
        while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
        DMA_Cmd(DMA1_Channel6, ENABLE);
    }
    else {
        // ===== SH1106：启动 DMA 链，从第 0 页开始 =====
        dma_chain_page = 0;
        dma_chain_busy = 1;
        SH1106_SendPage(p_data, 0);  // 发送第 0 页（128 B）
    }

    // STOP 会在 DMA 传输完成中断里自动发送
	
	memcpy(DRAW_BUF,SEND_BUF, sizeof(DRAW_BUF));//和上一帧保持一致
	if(OledOptions.DisplayReverse)	OLED_ReverseArea(0, 0, 64, 128);	//反色显示
	Update_Count++;	//用于读取屏幕刷新率
}


/**
  * @brief  将缓存数组全部更新到显示屏上,通信协议开销较大
  * @param  无
  * @retval 无
  */
void OLED_Update_soft(void)
{
	u8 page,x;
	for(page=0;page<8;page++)
	{
		OLED_SetCursor(page,0);
		for(x=0;x<128;x++)
		{
			OLED_WriteData(DRAW_BUF[page][x]);
		}
	}
}

/**
* @brief  将缓存数组部分区域更新到显示屏上,与DMA硬件不兼容
  * @param  Height最好为8的倍数,更新行单位最小为页
  * @tips   暂未加入边界检验
  */
void OLED_UpdateArea(uint8_t Line, uint8_t Column,uint8_t Height,uint8_t Width)
{
	u8 page_start,page_end;//,offset_bit
	page_start=Line/8;
	page_end=(Line+Height-1)/8;
	//offset_bit=(Line+Height-1)%8;
	OLED_SetCursor(page_start, Column);
	for(u8 page=page_start;page<=page_end;page++)
	{
		//OLED_SetCursor(page,Column);
		for(u8 x=Column;x<Column+Width;x++)
		{
			OLED_WriteData(DRAW_BUF[page][x]);
		}
	}
}

/**
  * @brief  在OLED缓存数组指定行和列擦除再写入指定高和宽的图像,这是一个底层函数
  * @param  Line 行位置，正向范围：0~63
  * @param  Column 列位置，正向范围：0~127
  * @param  Width 宽度	1~128
  * @param  Height 高度 1~64
  * @param  Image 要显示的一维图像数组，范围：ASCII可见字符
  * @param  Mode 模式0:直接写入;模式1:覆盖写入
*/

void OLED_ShowImage(int8_t Line, int8_t Column,uint8_t Height,uint8_t Width, const u8 *Image,uint8_t Mode)//image一维数组
{      	
	uint8_t i,j,temp;
	int8_t RealLine=Line;if(Line<0){Line=8+(Line%8);}//把负的 Line 映射到 0~7 之间,例如-2变6,-5变3
	u8 page = (Height - 1)/8 + 1;	 //页向上取整
	u8 MaxWidth=Column+Width>127?128-Column:Width;//图像右侧边界处理.须满足条件MaxWidth > Width
	if (MaxWidth > Width) MaxWidth=0;///右侧边界处理.须满足条件MaxWidth > Width
	
	for(i = 0;i < MaxWidth; i++)//j:页,由图像的高度决定写入几页;
	{
		if(Column  + i <0 ) continue;	//防止写入区域行不在屏幕坐标内,与offsetupdate函数协作动画
		for (j=0;j < page;j++)
		{
			if(RealLine+(j*8)<=-8) continue;	//列边界判定:防止写入区域行超出屏幕坐标内,与offsetupdate函数协作动画
			//擦除数据
			if(Mode)	//是否覆盖写入
			{
				if(Height % 8 && j==page-1)	//最后一页且Height不为8倍整,对图像进行底部裁切
				{
					temp=0xFF >> (8 - (Height % 8));
					DRAW_BUF[RealLine / 8 +j][Column  + i] &= ~(temp << (Line % 8));			//全选矩形图像数据清空覆盖,再填入数据
					DRAW_BUF[RealLine / 8 + 1 + j][Column  + i] &= ~(temp >> (8 - (Line%8)));  //
				}
				else
				{
					DRAW_BUF[RealLine / 8 +j][Column  + i] &= ~(0xFF << (Line % 8));			//全选矩形图像数据清空覆盖,再填入数据
					DRAW_BUF[RealLine / 8 + 1 + j][Column  + i] &= ~(0xFF >> (8 - (Line % 8)));  //
				}
			}
			//写入数据
			if(RealLine % 8 <0)//当满足以下条件:1.RealLine负数,2.不是8倍数
			{
				if(RealLine / 8 +j == 0)//在写入OLED的第一页时,补全缺失的字节
				{
					DRAW_BUF[RealLine / 8 +j][Column  + i] &= ~(0xFF >> (8 - (Line % 8)));			//步骤1
					DRAW_BUF[RealLine / 8 +j][Column  + i] |= Image[j*Width + i] >> (8 - (Line % 8));//步骤2

				}
				if(j+1<page)//if条件防止写入OLED最后一页的j越界造成Image取模溢出
				{
					DRAW_BUF[RealLine / 8 +j][Column  + i] |= Image[(j+1)*Width + i] << (Line % 8);			//显示跨页的上半部分内容
					DRAW_BUF[RealLine / 8 + 1 + j][Column  + i] |= Image[(j+1)*Width + i] >> (8 - (Line % 8));  //下一行显示跨页的下半部分内容
				}
			}
			else//正常条件,传入的偏移行是整数或偏移行是负数但为八倍
			{
				if(Height%8 && j==page-1)	//最后一页且Height不为8倍整
				{
					temp = Image[j*Width + i] & (0xFF>>(8-(Height%8)));
					DRAW_BUF[RealLine / 8 +j][Column  + i] |= temp << (Line % 8);			//显示跨页的上半部分内容
					DRAW_BUF[RealLine / 8 + 1 + j][Column  + i] |= temp >> (8 - (Line % 8));  //下一行显示跨页的下半部分内容
				}
				else
				{
					DRAW_BUF[RealLine / 8 +j][Column  + i] |= Image[j*Width + i] << (Line % 8);			//显示跨页的上半部分内容
					DRAW_BUF[RealLine / 8 + 1 + j][Column  + i] |= Image[j*Width + i] >> (8 - (Line % 8));  //下一行显示跨页的下半部分内容
				}
			}
		}
	}
}


/*
  *
  * @brief  将整个缓存数组按指定参数偏移并更新到显示屏上,用于制作动画
	若for循环调用本函数,上次修改的OLED_BUF会对本次产生累加偏移
	未针对列偏移做优化.可能不稳定
  * @param  Offset_Line 偏移行
  * @param  Offset_Column 偏移列
  * @retval 无
  */
void OLED_OffsetUpdate(int8_t Offset_Line,int8_t Offset_Column)
{
	u8 cache[8*128];
	memcpy(cache,DRAW_BUF, sizeof(DRAW_BUF));
	uint8_t Height=(Offset_Line>0?64-Offset_Line:64);//略去不必要的点阵行渲染可以节省性能开销
	//if(Offset_Line>0) OLED_ClearBufArea(0,0,Offset_Line,128);			//针对行偏移优化:向下偏移了几行,就给未擦除的几行手动擦除覆盖
	//if(Offset_Line<0) OLED_ClearBufArea(0,0,8,128);
	
	OLED_ShowImage(Offset_Line, Offset_Column,Height,128, cache,1);//擦除写入模式;可更换叠加写入模式
	OLED_Update();
}



/**
* @brief  在指定坐标画一个点
  * @param  Line      起始行 0~63
  * @param  Column    起始列 0~127
  */
void OLED_DrawPixel(int8_t Line, int8_t Column)
{
	DRAW_BUF[Line / 8][Column ] |= 0x01 << (Line % 8);
}

/**
  * @brief  Bresenham画线算法（支持虚实线）
  * @param  is_dashed: 0=实线, 1=虚线
  */
void OLED_DrawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t is_dashed)
{
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    #define DASH_STEP 2
    // 计算线段长度（用于虚线步进）
    float length = sqrtf(dx * dx + dy * dy);
    int16_t step_cnt = 0;
    int16_t step_target = 0;

    while (1) {
        // 边界保护
        if (x1 >= 0 && x1 < 128 && y1 >= 0 && y1 < 64) {
            // 虚线逻辑：间隔绘制
            if (!is_dashed || (step_cnt % (DASH_STEP * 2) < DASH_STEP)) {
                OLED_DrawPixel(y1, x1);
            }
        }
        
        if (x1 == x2 && y1 == y2) break;
        
        int16_t e2 = err * 2;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
        
        step_cnt++;
    }
}

/**
* @brief  在OLED缓存数组内画一个实心矩形（填充）,以OLED_ShowImage为底层函数,偷懒做法
  * @param  Line      屏幕可见起始行 0~63（负值按原规则映射）
  * @param  Column    屏幕可见起始列 0~127
  * @param  Height    矩形高度 1~64
  * @param  Width     矩形宽度 1~128
  * @param  Reverse   0=亮矩形(填0xFF)  1=暗矩形(填0x00)
  * @note   沿用OLED_ShowImage的边界、负坐标、跨页处理,Height自动填充向8倍取整
  */
void OLED_FillRect(int8_t Line, int8_t Column, uint8_t Height, uint8_t Width, u8 Reverse)
{
	u8 page =(Height>>3) +1;
	u8 cache[page][Width];
	if (Reverse){memset(cache,0,sizeof(cache));}
	else {memset(cache,0xFF,sizeof(cache));}
	OLED_ShowImage(Line,Column,Height,Width,(const u8*)cache,1);
	
}

/**
* @brief  在指定位置按给定的长和宽形成的矩形区域反色显示
  * @param  Line      起始行 0~63（负值按你原规则映射）
  * @param  Column    起始列 0~127
  * @param  Height    矩形高度 1~64
  * @param  Width     矩形宽度 1~128
  * @param  Reverse   0=亮矩形(填0xFF)  1=暗矩形(填0x00)
	* @note   沿用OLED_ShowImage的边界、负坐标、跨页处理,Height自动填充向8倍取整
  */
void OLED_ReverseArea(int8_t Line, int8_t Column, uint8_t Height, uint8_t Width)
{
    uint8_t i, j;
    uint8_t page_start = Line / 8;
    uint8_t page_end   = (Line + Height) / 8;
    uint8_t MaxWidth   = Column + Width > 127 ? 128 - Column : Width;
    if (MaxWidth > Width) MaxWidth = 0;

    /* 头页掩码：只反色从 Line%8 到页尾的行 */
    uint8_t headMask = 0xFF << (Line % 8);

    /* 尾页掩码：只反色从页首到 (Line+Height)%8 的行 */
    uint8_t tailMask = 0xFF >> (8 - ((Line + Height) % 8));

    if (page_start == page_end)                 /* 仅一页 */
    {
        uint8_t mask = headMask & tailMask;     /* 同一页内交集 */
        for (i = 0; i < MaxWidth; i++)
            DRAW_BUF[page_start][Column + i] ^= mask;
    }
    else                                        /* 多页 */
    {
        /* 1. 头页：部分反色 */
        for (i = 0; i < MaxWidth; i++)
            DRAW_BUF[page_start][Column + i] ^= headMask;

        /* 2. 尾页：部分反色 */
        for (i = 0; i < MaxWidth; i++)
            DRAW_BUF[page_end][Column + i] ^= tailMask;

        /* 3. 中间整页：全页反色 */
        for (j = page_start + 1; j < page_end; j++)
        {
            uint8_t *p = &DRAW_BUF[j][Column];
            for (i = 0; i < MaxWidth; i++)
                p[i] ^= 0xFF;                   /* 整页取反 */
        }
    }
}

/**
* @brief  以ShowImage函数为底座构建的字符函数
  * @param  Line 行位置，范围：0~63
  * @param  Column 列位置，范围：0~127
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @param  ... 不定长参数,填写高度和宽度
  * @retval 无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column,char Char,...)
{      
	va_list ap;
    va_start(ap, Char);

    uint8_t Height = va_arg(ap, int);   // 默认 promot 到 int
    uint8_t Width  = va_arg(ap, int);
	u8 cache[4*32];
    va_end(ap);

    // 若 Height==0 或 Width==0 说明调用者没给，用默认 8×16 
    if (Height == 0 || Width == 0) {
        OLED_ShowImage(Line,Column,16,8,(uint8_t *)OLED_F8x16[Char - ' '],1);
		return;
    }
	else 
	{	
		if (Height ==16 && Width == 8)
		{
			OLED_ShowImage(Line,Column,16,8,(uint8_t *)OLED_F8x16[Char - ' '],1);
			return;
		}
		scale_1bit(OLED_F8x16[Char - ' '],16,8,cache,Height,Width);	//使用temp作为缓存
        // 否则用缩放算法 
		OLED_ShowImage(Line, Column, Height, Width,cache, 1);
    }
}
	
	
		



/**
  * @brief  OLED显示一个字符
  * @param  Line 行位置，范围：1~4
  * @param  Column 列位置，范围：1~16
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowChar16x8(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);		//设置光标位置在上半部分
	for (i = 0; i < 8; i++)
	{
		DRAW_BUF[(Line-1)*2][i] = OLED_F8x16[Char - ' '][i];			//显示上半部分内容
	}
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);	//设置光标位置在下半部分
	for (i = 0; i < 8; i++)
	{
		DRAW_BUF[(Line-1)*2 + 1][i] = OLED_F8x16[Char - ' '][i + 8];		//显示下半部分内容
	}
}



/**
  * @brief  OLED显示字符串
  * @param  Line 行位置，范围：0~63
  * @param  Column 列位置，范围：0~127
  * @param  String 要显示的字符串，范围：ASCII可见字符
  * @param  Reverse 是否反色显示
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String,uint8_t Reverse,...)
{
	va_list ap;
    va_start(ap, Reverse);

    uint8_t Height = va_arg(ap, int);   // 默认 promot 到 int
    uint8_t Width  = va_arg(ap, int);
	u8 cache[4*64];
    va_end(ap);
	//以上代码捕获可变参数值
	uint8_t i;
	
	if (Height == 0 || Width == 0) //无附加参数时
	{
		for (i = 0; String[i] != '\0'; i++)
		{
			OLED_ShowChar(Line, Column + 8*i, String[i],16,8);
		}
	}
	else
	{
		for (i = 0; String[i] != '\0'; i++)
		{
			OLED_ShowChar(Line, Column + Width*i, String[i],Height,Width);
		}
	}
	

	if(Reverse) OLED_ReverseArea(Line,Column,16,8*i);
}

/**
  * @brief  OLED显示字符串【裁剪自江协开源】
  * @param  Line 行位置，范围：0~63
  * @param  Column 列位置，范围：0~127
  * @param  String 要显示的字符串，范围：字库字符
  * @param  Height 对字符进行缩放至指定高度
  * @param  Width 对字符进行缩放至指定宽度
  * @param  Mode 0是单字节模式,输出一字节后跳出循环,搭配printf函数格式化输出
  * @retval  1:写入数据包含中文字符;0:写入数据是纯ASCII码字符
  */
uint8_t OLED_ShowChinese(uint16_t Line, uint16_t Column, char *String,uint8_t Height,uint8_t Width,uint8_t Mode,uint8_t Reverse)
{
	uint16_t i = 0;
	char SingleChar[5];
	uint8_t CharLength = 0;
	uint16_t XOffset = 0;
	uint16_t pIndex;	//指针用于寻找中文字符
	uint8_t ChineseSign=0;	//判断本函数是否输出中文字符,若输出,则真,用于配合printf函数调优中文字符(占两字节)显示.
	while (String[i] != '\0')	//遍历字符串
	{
		
#ifdef OLED_CHARSET_GB2312						//定义字符集为GB2312
		/*此段代码的目的是，提取GB2312字符串中的一个字符，转存到SingleChar子字符串中*/
		/*判断GB2312字节的最高位标志位*/
		if ((String[i] & 0x80) == 0x00)			//最高位为0
		{
			CharLength = 1;						//字符为1字节
			SingleChar[0] = String[i ++];		//将第一个字节写入SingleChar第0个位置，随后i指向下一个字节
			SingleChar[1] = '\0';				//为SingleChar添加字符串结束标志位
		}
		else									//最高位为1
		{
			CharLength = 2;						//字符为2字节
			SingleChar[0] = String[i ++];		//将第一个字节写入SingleChar第0个位置，随后i指向下一个字节
			if (String[i] == '\0') {break;}		//意外情况，跳出循环，结束显示
			SingleChar[1] = String[i ++];		//将第二个字节写入SingleChar第1个位置，随后i指向下一个字节
			SingleChar[2] = '\0';				//为SingleChar添加字符串结束标志位
		}
#endif
		
		/*显示上述代码提取到的SingleChar*/
		if (CharLength == 1)	//如果是单字节字符
		{
			/*使用OLED_ShowChar显示此字符*/
			OLED_ShowChar(Line , Column+ XOffset, SingleChar[0],Height,Width);
			XOffset += Width;	//ASCII字符也共同缩放长宽
		}
		else					//否则，即多字节字符
		{
			/*遍历整个字模库，从字模库中寻找此字符的数据*/
			/*如果找到最后一个字符（定义为空字符串），则表示字符未在字模库定义，停止寻找*/
			for (pIndex = 0; strcmp(OLED_CF16x16[pIndex].Index, "") != 0; pIndex ++)
			{
				/*找到匹配的字符*/
				if (strcmp(OLED_CF16x16[pIndex].Index, SingleChar) == 0)
				{
					break;		//跳出循环，此时pIndex的值为指定字符的索引
				}
			}
			/*将字模库OLED_CF16x16的指定数据以16*16的图像格式显示*/
			if (Height ==16 && Width == 16)
			{
				OLED_ShowImage(Line, Column+ XOffset, 16, 16, OLED_CF16x16[pIndex].Data,1);
			}
			else// 否则用缩放算法缩放至相应大小
			{
				u8 cache[3*32];
				scale_1bit(OLED_CF16x16[pIndex].Data,16,16,cache,Height,Width);	//使用temp作为缓存
			
				OLED_ShowImage(Line, Column+ XOffset, Height, Width,cache, 1);
			}
			ChineseSign=1;	//中文字符输出标志
			XOffset += Width;
		}
		if(!Mode){break;}//如果为单字节模式,输出一字节后跳出循环
	}
	if (Reverse)//反色判断
	{
		OLED_ReverseArea(Line,Column,16,XOffset);
	}
	return ChineseSign;//返回字符串中是否带有中文字符，配合printf使用。
}


/**
* @brief  OLED自动换行显示字符串（支持整词换行）
  * @param  Line 起始行位置（像素坐标，范围：0~63）
  * @param  Column 起始列位置（像素坐标，范围：0~127）
  * @param  Height 字符高度（像素）
  * @param  Width 字符宽度（像素，中英文同宽）
  * @param  String 要显示的字符串（GB2312编码）
  * @param  lineLen 每行最大字符数
  * @param  Reverse 是否反色显示
  * @retval 是否包含中文字符
  * @note   英文单词不会在中间被截断，若单词超长则整体移至下一行【大小400B】
  */

uint8_t OLED_AutoWrappingString(uint16_t Line, uint16_t Column, uint8_t Height, uint8_t Width, 
                                char *String, uint8_t lineLen, uint8_t Reverse)
{
    uint16_t y = Line;          // 当前像素行（0, 16, 32...）
    uint8_t col = 0;            // 当前行已显示字符计数（0起）
    uint8_t hasChinese = 0;
    uint16_t i = 0;
    char buf[4];
    
    // 【关键】将像素宽度限制转换为字符数限制
    uint8_t max_col = lineLen / Width;
    if (max_col == 0) max_col = 1;  // 防除零，至少显示1个

    while (String[i] != '\0')
    {
        uint8_t char_len = 1;   // 字节长度（中文2，ASCII1）
        uint8_t is_space = 0;   // 当前是否为空格
        
        // 1. 解析字符类型（GB2312）
#ifdef OLED_CHARSET_GB2312
        if (String[i] & 0x80) {
            char_len = 2;
            hasChinese = 1;
        } else 
#endif
        {
            if (String[i] == ' ') is_space = 1;
        }

        // 2. 行首自动去空格（换行后的前导空格不显示）
        if (col == 0 && is_space) {
            i++;
            continue;
        }

        // 3. 【整词换行】仅当当前是单词首字符（非空格ASCII）且不在行首时检测
        // 注意：仅在确认当前字符不是空格时才触发单词长度预判
        if (!is_space && char_len == 1 && col > 0) {
            // 向前探测：计算该单词剩余长度（到下一个空格/中文/结束）
            uint16_t j = i;
            uint8_t word_len = 0;
            while (String[j] != '\0' && String[j] != ' ' && !(String[j] & 0x80)) {
                word_len++;
                j++;
            }
            
            // 如果当前行剩余字符数 < 单词长度，且单词本身不超行宽，则整词换行
            if (col + word_len > max_col && word_len <= max_col) {
                y += Height;        // 换到下一行像素位置
                if (y >= 64) break; // 超屏保护
                col = 0;            // 列计数清零
                continue;           // 【关键】i不增加，在新行重新处理该字符
            }
            // 若单词长度 > max_col（超长单词），不拦截，走下方强制截断逻辑
        }

        // 4. 普通溢出检查（当前行已满，或超长单词强制换行）
        if (col >= max_col) {
            y += Height;
            if (y >= 64) break;
            col = 0;
            continue;               // i不增加，新行重试该字符
        }

        // 5. 提取字符到缓冲区（中文防越界）
        if (char_len == 2) {
            if (String[i+1] == '\0') break;  // 防止半个中文
            buf[0] = String[i];
            buf[1] = String[i+1];
            buf[2] = '\0';
        } else {
            buf[0] = String[i];
            buf[1] = '\0';
        }

        // 6. 显示（Mode=0：单字节模式，仅画当前字符）
        OLED_ShowChinese(y, Column + col * Width, buf, Height, Width, 0, 0);
        
        i += char_len;  // 索引前进（中文+2，英文+1）
        col++;          // 列计数+1（每个字符占一个Width宽度）
    }

    // 7. 反色处理（精确计算实际占用行数）
    if (Reverse && col > 0) {
        uint8_t total_lines = (y - Line) / Height + 1;
        // 最后一行实际宽度为 col*Width，前面行都是 max_col*Width
        // 简化处理：整区域反色（包含未填充部分）
        OLED_ReverseArea(Line, Column, total_lines * Height, max_col * Width);
    }
    
    return hasChinese;
}

/**
* @brief  OLED显示格式化字符串,支持中文显示,ASCII显示效果不佳
  * @param  Line 行位置，范围：0~63
  * @param  Column 列位置，范围：0~127
  * @param  Height 字符串缩放高度
  * @param  Width 字符串缩放宽度
  * @param  String 要显示的字符串，范围：字库字符
  * @param  Reverse 是否反色显示
  * @retval 【裁剪自江协开源】
  */
void OLED_Printf(int8_t Line, uint8_t Column,uint8_t Height,uint8_t Width,
                 uint8_t Reverse, char *fmt, ...)
{
	va_list ap;
    va_start(ap, fmt);
	u8 Column0=Column;
	char output_str[20] = {0}; // 初始化为空
	int idx = 0;
	
    while (*fmt) {
        if (*fmt != '%') 
		{                     // 普通字符
            OLED_ShowChar(Line, Column, *fmt,Height,Width);
			output_str[idx++] = *fmt;
            Column += Width;
			fmt++;
            continue;
        }
        ++fmt;                               // 跳过 '%'
        /* 解析宽度 */
        uint8_t width = 0;
        while (*fmt >= '0' && *fmt <= '9') {   // 只支持数字宽度
            width = width * 10 + (*fmt - '0');
            ++fmt;
        }
        if (width == 0) width = 1;           // 默认长度 1

        switch (*fmt) {
        case 'd': {                          // 有符号十进制
            int16_t val = va_arg(ap, int);
            uint8_t  neg = 0;
            if (val < 0) { neg = 1; val = -val; }
            uint32_t uval = val;
            uint8_t  digits = 1;
            uint16_t tmp = uval;
            while (tmp > 9) { tmp /= 10; digits++; }
            uint8_t zero = (width > digits) ? width - digits : 0;
            if (neg) { zero ? zero-- : 0; } // 负号占 1 位
            while (zero--) { OLED_ShowChar(Line, Column, '0',Height,Width); Column += Width; }
            if (neg) { OLED_ShowChar(Line, Column, '-',Height,Width); Column += Width; }
            OLED_ShowNum(Line, Column, uval, digits, Height,Width,0);
			output_str[idx++] = (char)uval;
            Column += digits * Width;
            break;
        }
        case 'c': {                          // 字符
            char c = va_arg(ap, int);
			output_str[idx++] = c;
            OLED_ShowChar(Line, Column, c,Height,Width);
            Column += Width;
            break;
        }
        case 's': {                          // 字符串
            char *s = va_arg(ap, char *);
            while (*s) {
				output_str[idx++] = *s;
                if (OLED_ShowChinese(Line, Column, s++,Height,Width,0,0))
				{
					s++;	//如果s指向当前字符是中文，则s指针要额外加1（GB码非UTF-8,否则额外+2）
				}
                Column += Width;
            }
            break;
        }
        default:                             // 非法格式符，原样输出
			OLED_ShowChinese(Line, Column, fmt,Height,Width,0,0);
			OLED_ShowChinese(Line, Column, fmt,Height,Width,0,0);
            Column += Width;
            break;
        }
        ++fmt;
    }
	
	//OLED_ShowString(40,20,output_str,0);	//调试
    if (Reverse) {  // 整段反色
        uint8_t width = Column - Column0;
        OLED_ReverseArea(Line, Column0, Height, width);
    }
    va_end(ap);
}
/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
  * @brief  OLED显示数字（十进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 要显示数字的长度，范围：1~10
  * @param  Height 字符串缩放高度
  * @param  Width  字符串缩放宽度
  * @param  Reverse 是否反色显示

  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length,uint8_t Height,uint8_t Width,uint8_t Reverse)
{
	uint8_t i;
	if (Height==16 && Width==8)
	{
		for (i = 0; i < Length; i++)							
		{
			OLED_ShowChar(Line, Column + 8*i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0',16,8);
		}
	}
	else
	{
		for (i = 0; i < Length; i++)							
		{
			OLED_ShowChar(Line, Column + Width*i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0',Height,Width);
		}
	}
	if(Reverse) OLED_ReverseArea(Line,Column,16,Width*i);
}

/**
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + 8*i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + 8*i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + 8*i, SingleNumber - 10 + 'A');
		}
	}
}

/**
  * @brief  OLED显示数字（二进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  Length 要显示数字的长度，范围：1~16
  * @retval 无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + 8*i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
	uint32_t i, j;
	OLED_I2C_Init();			//端口初始化
	Delay_ms(20);				//上电延时,等待VCC电平拉升
	OLED_HardWriteCommand(0x20);  // Set Memory Addressing Mode
	OLED_HardWriteCommand(0x00);  // Horizontal Addressing Mode

	OLED_HardWriteCommand(0xAE);	//关闭显示
	if(g_oled_type == OLED_SH1106)	//SH1106初始化片段
	{
		OLED_HardWriteCommand(0x21);  // 进入“设置列地址”模式
		OLED_HardWriteCommand(0x00);  // 起始列 = 0
		OLED_HardWriteCommand(0x7F);  // 结束列 = 127 (0x7F = 127)
	}
	OLED_HardWriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_HardWriteCommand(0x80);
	OLED_HardWriteCommand(0xA8);	//设置多路复用率
	OLED_HardWriteCommand(0x3F);
	OLED_HardWriteCommand(0xD3);	//设置显示偏移
	OLED_HardWriteCommand(0x00);
	OLED_HardWriteCommand(0x40);	//设置显示开始行
	OLED_HardWriteCommand(0xA1);	//设置左右方向，0xA1正常 0xA0左右反置
	OLED_HardWriteCommand(0xC8);	//设置上下方向，0xC8正常 0xC0上下反置
	OLED_HardWriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_HardWriteCommand(0x12);
	OLED_HardWriteCommand(0x81);	//设置对比度控制
	OLED_HardWriteCommand(0xCF);
	OLED_HardWriteCommand(0xD9);	//设置预充电周期
	OLED_HardWriteCommand(0xF1);
	OLED_HardWriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_HardWriteCommand(0x30);
	OLED_HardWriteCommand(0xA4);	//设置整个显示打开/关闭
	OLED_HardWriteCommand(0xA6);	//设置正常/倒转显示
	OLED_HardWriteCommand(0x8D);	//设置充电泵
	OLED_HardWriteCommand(0x14);
	//打开屏幕显示
	OLED_HardWriteCommand(0xAF);
	OLED_Clear();				//OLED清屏
}

//OLED显示异常时使用该函数重新初始化屏幕
void OLED_Refresh(void)
{
	SystemInit();
	#include "Delay.h"
	
	GPIO_ResetBits(GPIOB, GPIO_Pin_5);
	Delay_ms(1);
	GPIO_SetBits(GPIOB, GPIO_Pin_5);
	Delay_ms(1);
	OLED_I2C_Init();	//低速触发卡死
	//OLED_Init();
	//OLED_Update();
	
}



/**
  * 函    数：OLED画椭圆,江科大代码
  * 参    数：X 指定椭圆的圆心横坐标，范围：-32768~32767，屏幕区域：0~127
  * 参    数：Y 指定椭圆的圆心纵坐标，范围：-32768~32767，屏幕区域：0~63
  * 参    数：A 指定椭圆的横向半轴长度，范围：0~255
  * 参    数：B 指定椭圆的纵向半轴长度，范围：0~255
  * 参    数：IsFilled 指定椭圆是否填充
  *           范围：OLED_UNFILLED		不填充
  *                 OLED_FILLED			填充
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled)
{
	int16_t x, y, j;
	int16_t a = A, b = B;
	float d1, d2;
	
	/*使用Bresenham算法画椭圆，可以避免部分耗时的浮点运算，效率更高*/
	/*参考链接：https://blog.csdn.net/myf_666/article/details/128167392*/
	
	x = 0;
	y = b;
	d1 = b * b + a * a * (-b + 0.5);
	
	if (IsFilled)	//指定椭圆填充
	{
		/*遍历起始点Y坐标*/
		for (j = -y; j < y; j ++)
		{
			/*在指定区域画点，填充部分椭圆*/
			OLED_DrawPixel( Y + j,X);
			OLED_DrawPixel( Y + j,X);
		}
	}
	
	/*画椭圆弧的起始点*/
	OLED_DrawPixel(Y + y,X + x );
	OLED_DrawPixel(Y - y,X - x);
	OLED_DrawPixel(Y + y,X - x);
	OLED_DrawPixel(Y - y,X + x);
	
	/*画椭圆中间部分*/
	while (b * b * (x + 1) < a * a * (y - 0.5))
	{
		if (d1 <= 0)		//下一个点在当前点东方
		{
			d1 += b * b * (2 * x + 3);
		}
		else				//下一个点在当前点东南方
		{
			d1 += b * b * (2 * x + 3) + a * a * (-2 * y + 2);
			y --;
		}
		x ++;
		
		if (IsFilled)	//指定椭圆填充
		{
			/*遍历中间部分*/
			for (j = -y; j < y; j ++)
			{
				/*在指定区域画点，填充部分椭圆*/
				OLED_DrawPixel(Y + j,X + x);
				OLED_DrawPixel(Y + j,X - x);
			}
		}
		
		/*画椭圆中间部分圆弧*/
		OLED_DrawPixel(Y + y,X + x );
		OLED_DrawPixel(Y - y,X - x );
		OLED_DrawPixel(Y + y,X - x );
		OLED_DrawPixel(Y - y,X + x);
	}
	
	/*画椭圆两侧部分*/
	d2 = b * b * (x + 0.5) * (x + 0.5) + a * a * (y - 1) * (y - 1) - a * a * b * b;
	
	while (y > 0)
	{
		if (d2 <= 0)		//下一个点在当前点东方
		{
			d2 += b * b * (2 * x + 2) + a * a * (-2 * y + 3);
			x ++;
			
		}
		else				//下一个点在当前点东南方
		{
			d2 += a * a * (-2 * y + 3);
		}
		y --;
		
		if (IsFilled)	//指定椭圆填充
		{
			/*遍历两侧部分*/
			for (j = -y; j < y; j ++)
			{
				/*在指定区域画点，填充部分椭圆*/
				OLED_DrawPixel(Y + j,X + x);
				OLED_DrawPixel(Y + j,X - x );
			}
		}
		
		/*画椭圆两侧部分圆弧*/
		OLED_DrawPixel(Y + y,X + x);
		OLED_DrawPixel(Y - y,X - x);
		OLED_DrawPixel(Y + y,X - x );
		OLED_DrawPixel(Y - y,X + x );
	}
}

//返回一段时间内屏幕刷新次数,用于测量帧率
uint8_t FPS_Get()
{
	u8 FPS;
	FPS = Update_Count;
	Update_Count=0;
	return FPS;
}
