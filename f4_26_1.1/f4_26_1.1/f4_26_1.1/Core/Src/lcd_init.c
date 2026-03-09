#include "lcd_init.h"

void Delay_us(uint32_t us) {
    __HAL_TIM_SET_COUNTER(&htim2, 0);  // 重置计数器
    __HAL_TIM_ENABLE(&htim2);          // 启动定时器
    
    while (__HAL_TIM_GET_COUNTER(&htim2) < us);  // 等待指定微秒
    
    __HAL_TIM_DISABLE(&htim2);         // 停止定时器
}

/****************************** 核心SPI传输函数 ******************************/
/**
 * @brief  向LCD总线写入数据（支持单字节/多字节批量传输）
 * @param  data: 数据缓冲区指针
 * @param  size: 传输字节数
 * @note   自动控制CS引脚，根据数据量选择DMA/阻塞模式
 */
void LCD_Writ_Bus(uint8_t *data, uint16_t size) 
{
    if (size == 0 || data == NULL) return;  // 无效参数检查
    
    HAL_GPIO_WritePin(lcd_cs_GPIO_Port, lcd_cs_Pin, GPIO_PIN_RESET);  // 拉低CS使能
    
    // 修复：只对大数据量使用DMA，小数据量（<=32字节）使用阻塞传输更可靠
    // 避免局部变量在DMA传输完成前被销毁的问题
    if (size > 32) {
        // 大数据量用DMA传输
        HAL_SPI_Transmit_DMA(&hspi2, data, size);
        // 等待传输完成（带超时保护）
        u32 timeout = 500000;  // 增加超时阈值
        while (HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY && timeout--);
        // 超时后尝试中止传输，防止SPI状态混乱
        if (timeout == 0) {
            HAL_SPI_Abort(&hspi2);
        }
    } else {
        // 小数据量用阻塞模式，更稳定可靠
        HAL_SPI_Transmit(&hspi2, data, size, 100);
    }
    
    HAL_GPIO_WritePin(lcd_cs_GPIO_Port, lcd_cs_Pin, GPIO_PIN_SET);  // 拉高CS关闭
}


/****************************** 数据/命令写入函数 ******************************/
/**
 * @brief  向LCD写入8位数据
 * @param  dat: 8位数据
 */
void LCD_WR_DATA8(u8 dat)
{
    LCD_Writ_Bus(&dat, 1);  // 调用多字节函数（单字节模式）
}

/**
 * @brief  向LCD写入16位数据（分高8位和低8位传输）
 * @param  dat: 16位数据
 */
void LCD_WR_DATA(u16 dat)
{
    // 使用静态变量，避免DMA传输时局部变量被销毁的问题
    static u8 data[2];
    data[0] = dat >> 8;  // 高8位
    data[1] = dat & 0xFF;  // 低8位
    LCD_Writ_Bus(data, 2);  // 批量传输2字节，现在使用阻塞模式（<=32字节）
}

/**
 * @brief  向LCD写入命令
 * @param  dat: 8位命令
 * @note   通过DC引脚区分命令/数据（拉低DC为命令）
 */
void LCD_WR_REG(u8 dat)
{
    HAL_GPIO_WritePin(lcd_dc_GPIO_Port, lcd_dc_Pin, GPIO_PIN_RESET);  // DC=0（命令）
    LCD_Writ_Bus(&dat, 1);  // 传输命令
    HAL_GPIO_WritePin(lcd_dc_GPIO_Port, lcd_dc_Pin, GPIO_PIN_SET);    // DC=1（恢复数据模式）
}


/****************************** 地址设置与填充函数 ******************************/
/**
 * @brief  设置LCD显示区域（坐标范围）
 * @param  x1,y1: 起始坐标
 * @param  x2,y2: 结束坐标
 */
void LCD_Address_Set(u16 x1, u16 y1, u16 x2, u16 y2)
{
    LCD_WR_REG(0x2A);  // 列地址设置命令
    LCD_WR_DATA(x1);   // 起始列
    LCD_WR_DATA(x2);   // 结束列
    
    LCD_WR_REG(0x2B);  // 行地址设置命令
    LCD_WR_DATA(y1);   // 起始行
    LCD_WR_DATA(y2);   // 结束行
    
    LCD_WR_REG(0x2C);  // 进入数据写入模式
}
void LCD_Init(void)
{
	
	HAL_GPIO_WritePin(lcd_res_GPIO_Port, lcd_res_Pin, GPIO_PIN_RESET);//¸´Î»
	HAL_Delay(100);
	HAL_GPIO_WritePin(lcd_res_GPIO_Port, lcd_res_Pin, GPIO_PIN_SET);
	HAL_Delay(100);
	
//	HAL_GPIO_WritePin(lcd_blk_GPIO_Port, lcd_blk_Pin, GPIO_PIN_SET);//´ò¿ª±³¹â
	HAL_Delay(100);
	
	LCD_WR_REG(0xEF);
	LCD_WR_REG(0xEB);
	LCD_WR_DATA8(0x14); 
	
	LCD_WR_REG(0xFE);			 
	LCD_WR_REG(0xEF); 

	LCD_WR_REG(0xEB);	
	LCD_WR_DATA8(0x14); 

	LCD_WR_REG(0x84);			
	LCD_WR_DATA8(0x40); 

	LCD_WR_REG(0x85);			
	LCD_WR_DATA8(0xFF); 

	LCD_WR_REG(0x86);			
	LCD_WR_DATA8(0xFF); 

	LCD_WR_REG(0x87);			
	LCD_WR_DATA8(0xFF);

	LCD_WR_REG(0x88);			
	LCD_WR_DATA8(0x0A);

	LCD_WR_REG(0x89);			
	LCD_WR_DATA8(0x21); 

	LCD_WR_REG(0x8A);			
	LCD_WR_DATA8(0x00); 

	LCD_WR_REG(0x8B);			
	LCD_WR_DATA8(0x80); 

	LCD_WR_REG(0x8C);			
	LCD_WR_DATA8(0x01); 

	LCD_WR_REG(0x8D);			
	LCD_WR_DATA8(0x01); 

	LCD_WR_REG(0x8E);			
	LCD_WR_DATA8(0xFF); 

	LCD_WR_REG(0x8F);			
	LCD_WR_DATA8(0xFF); 


	LCD_WR_REG(0xB6);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x20);

	LCD_WR_REG(0x36);
	if(USE_HORIZONTAL==0)LCD_WR_DATA8(0x08);
	else if(USE_HORIZONTAL==1)LCD_WR_DATA8(0xC8);
	else if(USE_HORIZONTAL==2)LCD_WR_DATA8(0x68);
	else LCD_WR_DATA8(0xA8);

	LCD_WR_REG(0x3A);			
	LCD_WR_DATA8(0x05); 


	LCD_WR_REG(0x90);			
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x08); 

	LCD_WR_REG(0xBD);			
	LCD_WR_DATA8(0x06);
	
	LCD_WR_REG(0xBC);			
	LCD_WR_DATA8(0x00);	

	LCD_WR_REG(0xFF);			
	LCD_WR_DATA8(0x60);
	LCD_WR_DATA8(0x01);
	LCD_WR_DATA8(0x04);

	LCD_WR_REG(0xC3);			
	LCD_WR_DATA8(0x13);
	LCD_WR_REG(0xC4);			
	LCD_WR_DATA8(0x13);

	LCD_WR_REG(0xC9);			
	LCD_WR_DATA8(0x22);

	LCD_WR_REG(0xBE);			
	LCD_WR_DATA8(0x11); 

	LCD_WR_REG(0xE1);			
	LCD_WR_DATA8(0x10);
	LCD_WR_DATA8(0x0E);

	LCD_WR_REG(0xDF);			
	LCD_WR_DATA8(0x21);
	LCD_WR_DATA8(0x0c);
	LCD_WR_DATA8(0x02);

	LCD_WR_REG(0xF0);   
	LCD_WR_DATA8(0x45);
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x26);
 	LCD_WR_DATA8(0x2A);

 	LCD_WR_REG(0xF1);    
 	LCD_WR_DATA8(0x43);
 	LCD_WR_DATA8(0x70);
 	LCD_WR_DATA8(0x72);
 	LCD_WR_DATA8(0x36);
 	LCD_WR_DATA8(0x37);  
 	LCD_WR_DATA8(0x6F);


 	LCD_WR_REG(0xF2);   
 	LCD_WR_DATA8(0x45);
 	LCD_WR_DATA8(0x09);
 	LCD_WR_DATA8(0x08);
 	LCD_WR_DATA8(0x08);
 	LCD_WR_DATA8(0x26);
 	LCD_WR_DATA8(0x2A);

 	LCD_WR_REG(0xF3);   
 	LCD_WR_DATA8(0x43);
 	LCD_WR_DATA8(0x70);
 	LCD_WR_DATA8(0x72);
 	LCD_WR_DATA8(0x36);
 	LCD_WR_DATA8(0x37); 
 	LCD_WR_DATA8(0x6F);

	LCD_WR_REG(0xED);	
	LCD_WR_DATA8(0x1B); 
	LCD_WR_DATA8(0x0B); 

	LCD_WR_REG(0xAE);			
	LCD_WR_DATA8(0x77);
	
	LCD_WR_REG(0xCD);			
	LCD_WR_DATA8(0x63);		


	LCD_WR_REG(0x70);			
	LCD_WR_DATA8(0x07);
	LCD_WR_DATA8(0x07);
	LCD_WR_DATA8(0x04);
	LCD_WR_DATA8(0x0E); 
	LCD_WR_DATA8(0x0F); 
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x07);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x03);

	LCD_WR_REG(0xE8);			
	LCD_WR_DATA8(0x34);

	LCD_WR_REG(0x62);			
	LCD_WR_DATA8(0x18);
	LCD_WR_DATA8(0x0D);
	LCD_WR_DATA8(0x71);
	LCD_WR_DATA8(0xED);
	LCD_WR_DATA8(0x70); 
	LCD_WR_DATA8(0x70);
	LCD_WR_DATA8(0x18);
	LCD_WR_DATA8(0x0F);
	LCD_WR_DATA8(0x71);
	LCD_WR_DATA8(0xEF);
	LCD_WR_DATA8(0x70); 
	LCD_WR_DATA8(0x70);

	LCD_WR_REG(0x63);			
	LCD_WR_DATA8(0x18);
	LCD_WR_DATA8(0x11);
	LCD_WR_DATA8(0x71);
	LCD_WR_DATA8(0xF1);
	LCD_WR_DATA8(0x70); 
	LCD_WR_DATA8(0x70);
	LCD_WR_DATA8(0x18);
	LCD_WR_DATA8(0x13);
	LCD_WR_DATA8(0x71);
	LCD_WR_DATA8(0xF3);
	LCD_WR_DATA8(0x70); 
	LCD_WR_DATA8(0x70);

	LCD_WR_REG(0x64);			
	LCD_WR_DATA8(0x28);
	LCD_WR_DATA8(0x29);
	LCD_WR_DATA8(0xF1);
	LCD_WR_DATA8(0x01);
	LCD_WR_DATA8(0xF1);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x07);

	LCD_WR_REG(0x66);			
	LCD_WR_DATA8(0x3C);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0xCD);
	LCD_WR_DATA8(0x67);
	LCD_WR_DATA8(0x45);
	LCD_WR_DATA8(0x45);
	LCD_WR_DATA8(0x10);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x00);

	LCD_WR_REG(0x67);			
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x3C);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x01);
	LCD_WR_DATA8(0x54);
	LCD_WR_DATA8(0x10);
	LCD_WR_DATA8(0x32);
	LCD_WR_DATA8(0x98);

	LCD_WR_REG(0x74);			
	LCD_WR_DATA8(0x10);	
	LCD_WR_DATA8(0x85);	
	LCD_WR_DATA8(0x80);
	LCD_WR_DATA8(0x00); 
	LCD_WR_DATA8(0x00); 
	LCD_WR_DATA8(0x4E);
	LCD_WR_DATA8(0x00);					
	
	LCD_WR_REG(0x98);			
	LCD_WR_DATA8(0x3e);
	LCD_WR_DATA8(0x07);

	LCD_WR_REG(0x35);	
	LCD_WR_REG(0x21);

	LCD_WR_REG(0x11);
	HAL_Delay(120);
	
	// 先清屏为黑色，避免显示随机数据（雪花屏）
	{
		LCD_Address_Set(0, 0, LCD_WIDTH-1, LCD_HEIGHT-1);
		#define CLEAR_BUF_SIZE 512
		uint8_t clear_buf[CLEAR_BUF_SIZE * 2] = {0};  // 全0就是黑色
		uint32_t total = LCD_WIDTH * LCD_HEIGHT;
		while(total > 0) {
			uint32_t send = (total > CLEAR_BUF_SIZE) ? CLEAR_BUF_SIZE : total;
			LCD_Writ_Bus(clear_buf, send * 2);
			total -= send;
		}
	}
	
	LCD_WR_REG(0x29);
	HAL_Delay(20);
}


