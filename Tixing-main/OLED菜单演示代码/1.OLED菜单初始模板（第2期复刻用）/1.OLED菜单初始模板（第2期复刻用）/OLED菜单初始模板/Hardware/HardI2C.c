/* ---------------- HardI2C.c ---------------- */
/* ---------------- 硬件I2C和引脚时钟配置,是协议层 ---------------- */
#include "stm32f10x.h"
#include "Delay.h"
#include "HardI2C.h"
#include "OLED.h"
/*引脚初始化*/
/* ---------------- 当启用硬件I2C刷新OLED屏幕时,启用以下函数 ---------------- */
#ifndef SoftI2C_ENABLED

#define OLED_I2C_ADDR   (0x3C<<1)   // 0x78 的 7 位形式
#define CMD_CTRL_BYTE   0x00        // Co=0, D/C#=0
#define DAT_CTRL_BYTE   0x40        // Co=0, D/C#=1
#define I2C_TIMEOUT  10000			//超时计数（根据主频调整)
static uint8_t  i2c_buf[2];         // 0:Ctrl  1:Payload
volatile uint8_t dma_done;   // 1=完成，0=正在传


// 添加全局变量
OLED_Type g_oled_type = OLED_SH1106;  // 默认 OLED_SSD1306,OLED_SH1106
// 添加 DMA 链状态变量
volatile uint8_t dma_chain_page = 0;      // 当前刷到第几页（SH1106）
volatile uint8_t dma_chain_busy = 0;      // DMA 链是否正在跑


// 私有函数声明 
static void I2C1_DMA_Send(uint8_t ctrl, uint8_t data);

// ----------------------------------------------------------
 // 1. 引脚 + I2C1 + DMA 初始化
 // ----------------------------------------------------------
void OLED_I2C_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    I2C_InitTypeDef   i2c;
    NVIC_InitTypeDef  nvic;

    // 1. 时钟 
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    //2. GPIO 配置和复用开漏 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_5);
	Delay_ms(5);
	
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);  // 仅禁用 JTAG，保留 SWD
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
	
    // 3. I2C1 配置 
    I2C_DeInit(I2C1);
    i2c.I2C_Mode                = I2C_Mode_I2C;
    i2c.I2C_DutyCycle           = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1         = 0x00;
    i2c.I2C_Ack                 = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    if(SystemCoreClock >= 64000000) {        // 64MHz 以上（如 72MHz）
        i2c.I2C_ClockSpeed = 1300000;  // 可超频到 1.3MHz
    } else if(SystemCoreClock >= 40000000) { // 40MHz 以上
        i2c.I2C_ClockSpeed = 1000000;   // 安全频率
    } else {                       // 24MHz 及以下
        i2c.I2C_ClockSpeed = 400000;   // 标准 Fast Mode
    }
    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);

    DMA_InitTypeDef dma;

    DMA_DeInit(DMA1_Channel6);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&I2C1->DR;   // 外设数据寄存器
    dma.DMA_MemoryBaseAddr     = (uint32_t)i2c_buf;     // 内存缓冲区
    dma.DMA_DIR                = DMA_DIR_PeripheralDST; // 内存→外设
    dma.DMA_BufferSize         = 2;                     // 固定 2 字节
    dma.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode               = DMA_Mode_Normal;
    dma.DMA_Priority           = DMA_Priority_VeryHigh;
    dma.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel6, &dma);

	dma_done = 1;          // 关键！第一次调用前必须=1
    DMA_ITConfig(DMA1_Channel6, DMA_IT_TC, ENABLE);     // 开传输完成中断

    // 5. NVIC for DMA1 Channel 6 
    nvic.NVIC_IRQChannel                   = DMA1_Channel6_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority        = 2;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}


//----------------------------------------------------------
 // 3. 通用 DMA 发送（2 字节）
 // ----------------------------------------------------------
static void I2C1_DMA_Send(uint8_t ctrl, uint8_t data)
{
	uint16_t timeout = 0x18;	//有现象增大这里到FFFF导致sh1106不显示??

	while (!dma_done)	//CPU 无法去做其他事情，失去了使用 DMA 解放 CPU 的核心意义。
    {
        if (--timeout == 0)
        {
            // 超时恢复：强制复位 I2C 和 DMA 
            DMA_Cmd(DMA1_Channel6, DISABLE);
            I2C_DMACmd(I2C1, DISABLE);
			//I2C_GenerateSTOP(I2C1, ENABLE);	//导致黑屏
            dma_done = 1;
            break;
        }
    }
	
    i2c_buf[0] = ctrl;
    i2c_buf[1] = data;

    dma_done = 0;
    DMA_ClearFlag(DMA1_FLAG_TC6);
    DMA1_Channel6->CNDTR = 2;   // 只搬 2 字节

    I2C_DMACmd(I2C1, ENABLE);   // 允许 I2C 请求 DMA

    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));//{uint32_t to = 0xFFFF;if (--to < 1) break;};

    I2C_Send7bitAddress(I2C1, OLED_I2C_ADDR, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));//{uint32_t to = 0xFFFF;if (--to < 1) break;};
    // 地址阶段完成后再开 DMA，搬数据阶段 
    DMA_Cmd(DMA1_Channel6, ENABLE);
}


// ----------------------------------------------------------
 // 5. DMA1 Channel6 中断服务函数
 // ----------------------------------------------------------
void DMA1_Channel6_IRQHandler(void)
{
	uint32_t timeout = 0;
    if (DMA_GetITStatus(DMA1_IT_TC6))	//传输完毕标志
    {
        
		DMA_ClearITPendingBit(DMA1_IT_TC6);	//清除传输完毕标志
        DMA_Cmd(DMA1_Channel6, DISABLE);	//dma失能
        I2C_DMACmd(I2C1, DISABLE);		//I2C的DMA通道失能
        // 等待最后一个字节真正发完 
        while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF));
		I2C_GenerateSTOP(I2C1, ENABLE);		//发送停止波形
		while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY)&& ++timeout < 5000);
		if(timeout >= 5000) 
		{
			// I2C 死锁，强制复位
			I2C_GenerateSTOP(I2C1, ENABLE); 
			I2C_SoftwareResetCmd(I2C1, ENABLE);
			for(volatile int i=0; i<100; i++) __NOP();
			I2C_SoftwareResetCmd(I2C1, DISABLE);
			dma_chain_busy = 0;
			dma_chain_page=0;
			return;  // 返回失败
		}
        dma_done = 1;	//dma标志
		// ===== 如果是 SH1106 且 DMA 链还没跑完，继续下一页 =====
        if(g_oled_type == OLED_SH1106 && dma_chain_busy) 
		{
            dma_chain_page++;
            if(dma_chain_page < 8) {
                // 还有下一页，继续发
                SH1106_SendPage((uint8_t*)SEND_BUF, dma_chain_page);	//考虑在主循环中用事件调度!
            } else {
                // 8 页全部发完
                dma_chain_busy = 0;
				dma_chain_page=0;
            }
        }
		
		
    }
	else if (DMA_GetITStatus(DMA1_IT_TE6)) // 传输错误
    {
        DMA_ClearITPendingBit(DMA1_IT_TE6);
        DMA_Cmd(DMA1_Channel6, DISABLE);
        I2C_DMACmd(I2C1, DISABLE);
        I2C_GenerateSTOP(I2C1, ENABLE);
        I2C_ClearFlag(I2C1, I2C_FLAG_AF); // 清除可能的应答错误
		dma_chain_busy = 0;
        dma_done = 1;
    }
	else
	{		
		dma_done = 1;	//dma标志
		dma_chain_busy = 0;
		I2C_ClearFlag(I2C1, I2C_FLAG_AF); // 清除可能的应答错误
	}
}

// ----------------------------------------------------------
 // 4. 写命令 / 写数据 对外接口（与旧代码完全兼容）
 // ----------------------------------------------------------
void OLED_WriteCommand(uint8_t Command)
{
    I2C1_DMA_Send(CMD_CTRL_BYTE,Command);
}

void OLED_WriteData(uint8_t data)
{
    I2C1_DMA_Send(DAT_CTRL_BYTE,data);
}


//硬件I2C直接写命令
void OLED_HardWriteCommand(uint8_t Command)
{
	uint32_t timeout;
	timeout = I2C_TIMEOUT;// 等待总线空闲（带超时）
	while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY)) {
        if (--timeout == 0) goto error_exit;
    }

    // 发送 START
    I2C_GenerateSTART(I2C1, ENABLE);
    timeout = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT)) {
        if (--timeout == 0) goto error_exit;
    }

    // 发送地址
    I2C_Send7bitAddress(I2C1, OLED_I2C_ADDR, I2C_Direction_Transmitter);
    timeout = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) {
        if (--timeout == 0) goto error_exit;
    }

    // 发送控制字节
    I2C_SendData(I2C1, 0x00);
    timeout = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        if (--timeout == 0) goto error_exit;
    }

    // 发送命令
    I2C_SendData(I2C1, Command);
    timeout = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) {
        if (--timeout == 0) goto error_exit;
    }

    // 等待 BTF（可选，建议保留但加超时）
    timeout = I2C_TIMEOUT / 2;  // BTF 应该很快
    while (!I2C_GetFlagStatus(I2C1, I2C_FLAG_BTF)) {
        if (--timeout == 0) break;  // BTF 超时不强求，继续发 STOP
    }

    // 发送 STOP
    I2C_GenerateSTOP(I2C1, ENABLE);
    
    // 最后等待 BUSY 清除（带超时，关键！）
    timeout = I2C_TIMEOUT;
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY)) {
        if (--timeout == 0) {
            // 强制复位 I2C 解除死锁
            I2C_SoftwareResetCmd(I2C1, ENABLE);
            for(volatile int i=0; i<100; i++);  // 短暂延时
            I2C_SoftwareResetCmd(I2C1, DISABLE);
            break;
        }
    }
    
    return;  // 成功返回

	error_exit:
		// 发生超时，发送 STOP 并复位 I2C
		I2C_GenerateSTOP(I2C1, ENABLE);
		I2C_SoftwareResetCmd(I2C1, ENABLE);
		for(volatile int i=0; i<100; i++);
		I2C_SoftwareResetCmd(I2C1, DISABLE);
}

#endif


// SH1106底层刷写链函数
void SH1106_SendPage(uint8_t *p_buf, uint8_t page)
{
	
	// 0. 等待DMA就绪
    uint32_t timeout = 10000;
    while (!dma_done && --timeout);  // 等待上一页完成
    if (timeout == 0) {
        // 超时恢复
        DMA_Cmd(DMA1_Channel6, DISABLE);
        I2C_DMACmd(I2C1, DISABLE);
        I2C_GenerateSTOP(I2C1, ENABLE);
        dma_done = 1;
    }
	
    // 1. 发命令：设置页地址和列地址 0
    uint8_t cmd[] = { 0xB0 | page, 0x02, 0x10 };
    OLED_HardWriteCommand(cmd[0]);  // 页地址
    OLED_HardWriteCommand(cmd[1]);  // 列地址低
    OLED_HardWriteCommand(cmd[2]);  // 列地址高

    // 2. 切数据模式，DMA 发 128 B
    DMA1_Channel6->CMAR = (uint32_t)(p_buf + page*128);
    DMA1_Channel6->CNDTR = 128;
    dma_done = 0;
    I2C_DMACmd(I2C1, ENABLE);
    I2C_GenerateSTART(I2C1, ENABLE);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));
    I2C_Send7bitAddress(I2C1, OLED_I2C_ADDR, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
    I2C_SendData(I2C1, DAT_CTRL_BYTE);  // 0x40
    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    DMA_Cmd(DMA1_Channel6, ENABLE);
}


/*
//不DMA转运的硬件I2C写,单字节写的通信协议开销很大
void OLED_WriteCommand(uint8_t Command)
{
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY)); // 等待总线空闲

    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, 0x78, I2C_Direction_Transmitter);//告诉硬件：接下来我要“写”数据到从机，而不是“读”数据。
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));//检测 “主机已经成功发送了从机地址 + 写位，并且从机回了 ACK” 

    I2C_SendData(I2C1, 0x00); // 发送命令字节
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C1, Command);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C1, ENABLE);
}


void OLED_WriteData(uint8_t data)
{
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY)); // 等待总线空闲

    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C1, 0x78, I2C_Direction_Transmitter);//告诉硬件：接下来我要“写”数据到从机，而不是“读”数据。
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));//检测 “主机已经成功发送了从机地址 + 写位，并且从机回了 ACK” 

    I2C_SendData(I2C1, 0x40); // 发送命令字节
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C1, data);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C1, ENABLE);
}
*/

/*

表面现象
修改事件总线框架后，注释掉EventBus_Publish(ev)导致SH1106 OLED黑屏，SSD1306却正常。
第一层机理：CPU空转饿死DMA中断
现象：主循环空转时，CPU以72MHz频率执行while(1)，几乎占满总线
机理：SH1106需要8次DMA链式传输完成刷屏，每次DMA中断需在1ms内触发下一次。CPU空转导致DMA中断响应延迟，错过传输窗口，SH1106只收到第1页数据就放弃，屏幕保持黑屏。

*/
