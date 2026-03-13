/* ---------------- Clock.c ---------------- */
/* ---------------- OLED手表时钟页 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include "OLED.h"
#include "Animation.h"
#include "MyTimer.h"
#include "Clock.h"
#include "Image.h"
#include "Key.h"
#include "MyRTC.h"
#include "Action.h"
#include "HardI2C.h"
#include "Encoder.h"
#include "Delay.h"

/*
编码器逆时针转:EVENT 1
编码器顺时针转:EVENT 2
按键中断1:EVENT 3
按键中断2:EVENT 4
*/
/* 按键事件值 */
#define ENC_CCW     2
#define ENC_CW      1
#define KEY_OK      4
#define KEY_BACK    3

typedef enum {
    CLOCK_READY,
    CLOCK_SLEEP,
} CLOCK_STATE;

/*---------- 雷达扫描参数（替换原轨道参数）----------*/
#define RADAR_CX        40      // 雷达中心X（偏左给右侧数据留位）
#define RADAR_CY        32      // 雷达中心Y
#define RADAR_R1        10      // 分钟圈半径（内圈）
#define RADAR_R2        20      // 小时圈半径（中圈）
#define RADAR_R3        30      // 扫描线半径（外圈）

/*---------- 海洋帆船参数 ----------*/
#define WAVE_Y1     40      // 第一层波浪基准线
#define WAVE_Y2     48      // 第二层波浪基准线  
#define WAVE_Y3     56      // 第三层波浪基准线
//#define SHIP_X    60      // 帆船固定X位置（或改为随秒移动的变量：-8 + (sec % 60) * 2.3）

/*---------- 全局变量 ----------*/
static CLOCK_STATE state = CLOCK_READY;
/*---------- 绘制同步标志 ----------*/
static volatile uint8_t g_is_drawing = 0;  // 0=空闲, 1=绘制中
static volatile uint8_t g_enter_sleep_pending = 0;  // 请求进入睡眠标志
/*---------- 动画状态变量（静态保持帧间状态）----------*/
static uint16_t wave_phase = 0;      // 波浪相位
static uint8_t cloud1_x = 120;       // 云朵1位置
static uint8_t cloud2_x = 50;        // 云朵2位置  
static uint8_t bird_x = 140;         // 海鸥位置（超出屏幕为隐藏）
static int8_t bird_y = 15;          // 海鸥高度
static int8_t SHIP_X = -8;          // 海鸥高度
static uint8_t anim_tick = 0;        // 动画节拍器

/*---------- 三角函数查表（保留，用于极坐标计算）----------*/
static const int8_t COS_TAB[60] = {
    100, 99, 97, 95, 92, 87, 81, 74, 67, 59,
    50, 41, 31, 21, 10, 0, -10, -21, -31, -41,
    -50, -59, -67, -74, -81, -87, -92, -95, -97, -99,
    -100, -99, -97, -95, -92, -87, -81, -74, -67, -59,
    -50, -41, -31, -21, -10, 0, 10, 21, 31, 41,
    50, 59, 67, 74, 81, 87, 92, 95, 97, 99
};

static const int8_t SIN_TAB[60] = {
    0, 10, 21, 31, 41, 50, 59, 67, 74, 81,
    87, 92, 95, 97, 99, 100, 99, 97, 95, 92,
    87, 81, 74, 67, 59, 50, 41, 31, 21, 10,
    0, -10, -21, -31, -41, -50, -59, -67, -74, -81,
    -87, -92, -95, -97, -99, -100, -99, -97, -95, -92,
    -87, -81, -74, -67, -59, -50, -41, -31, -21, -10
};

/*---------- 静态辅助函数声明 ----------*/
static void Get_Radar_Pos(uint8_t r, uint8_t angle_idx, int16_t *x, int16_t *y);
static void Draw_Line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
static int8_t iabs(int8_t x);
static void Draw_Cloud(int8_t x, int8_t y);
static void Draw_Bird(int8_t x, int8_t y);
static void Draw_Ship(int16_t x, int16_t y);
static void fillCircle(uint8_t y, uint8_t x, uint8_t r);
/*========== 对外接口 ==========*/
void CLOCK_Enter(void)
{
    state = CLOCK_READY;

	RCC_HSICmd(ENABLE);
	uint32_t t = 0;
	while (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) == RESET && ++t < 0x3000)
	{
		if (t >= 0x3000) 
		{
			break;
		}
	}
	/*
	RCC_SYSCLKConfig(RCC_SYSCLKSource_HSI);    
	SystemCoreClock = 8000000;
	SystemCoreClockUpdate();	

	RCC_PLLCmd(DISABLE);
	RCC_HSEConfig(RCC_HSE_OFF); 
	*/
	ActionDisplay_Init(CLOCK_Display);
	ActionKey_Init(CLOCK_KeyAction);
	
	Timer_SetPrescaler(100);	
	
	TaskTimer.UpdateTask = 1;
	
	
	//

}

void CLOCK_KeyAction(uint8_t event)
{
    switch (state) {
    case CLOCK_READY:
        if (event == KEY_OK) 
		{ 
			u32 t = 0;
			// 1. 停止新的Update事件
            TaskTimer.UpdateTask = 0;
            g_enter_sleep_pending = 1;
			
			/*
            // 2. 等待当前绘制完成（关键！现在g_is_drawing会正确置1了）
            while(g_is_drawing && ++t < 0x10000);
            
            // 3. 等待当前DMA传输完成（带超时）
            t = 0;
            while(dma_done == 0 && ++t < 0x40000);
            
            // 4. 如果超时，强制复位I2C总线（解决SDA被拉低死锁）
            if (t >= 0x40000) {
                DMA_Cmd(DMA1_Channel6, DISABLE);  // 停止DMA
                I2C_GenerateSTOP(I2C1, ENABLE);   // 发送停止位
                
                // 关键：I2C死锁恢复（发送9个时钟脉冲释放SDA）
                GPIO_InitTypeDef GPIO_InitStruct;
                GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7; // SCL/SDA
                GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出
                GPIO_Init(GPIOB, &GPIO_InitStruct);
                
                // 手动翻转SCL 9次
                for(int i = 0; i < 9; i++) {
                    GPIO_SetBits(GPIOB, GPIO_Pin_6);
                    Delay_us(5);
                    GPIO_ResetBits(GPIOB, GPIO_Pin_6);
                    Delay_us(5);
                }
                
                // 恢复I2C功能
                GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_OD;
                GPIO_Init(GPIOB, &GPIO_InitStruct);
                I2C_GenerateSTOP(I2C1, ENABLE);
                
                dma_done = 1;
            }
			OLED_WriteCommand(0xAE);	//OLED熄屏
			state=CLOCK_SLEEP;
			
			t=0;
			while (++t < 0xF000)
			{
			}
			
			TaskTimer.UpdateTask=1;
			*/
			//GPIO_LowPower_Config();
			//PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
			
		}
		else if (event == KEY_BACK) 
		{
			//时钟频率回调
			
			if(menuOptions.SystemClock==72)
			{
				SystemInit();
				menuOptions.SystemClock=72;
				SystemCoreClockUpdate();
			}
			else if(menuOptions.SystemClock==40)
			{
				SystemSET_SysClockConfig_Medium();
			}
			else if(menuOptions.SystemClock==24)
			{
				SystemSET_SysClockConfig_Low();
			}
			
			ActionKey_Free(CLOCK_KeyAction);
			ACTION_Exit(); //离开时钟页,回到菜单页
			
		}
        break;

	case CLOCK_SLEEP:	//进入停止模式
		if (event == KEY_OK || event == KEY_BACK) 
		{
			//NVIC_SystemReset();
			
			OLED_ClearBuf();
			TaskTimer.UpdateTask=0;
			//System_ExitStopMode();
			u16 t = 0;
			
			while ( dma_done == 0 && ++t < 0x1000 )	//DMA在工作中
			{
				if (t >= 0x1000) 
				{
					break;
				}
			}
			
			OLED_WriteCommand(0xAF);	//点亮屏幕
			
			while (++t < 0x3000)
			{
				if (t >= 0x3000) 
				{
					break;
				}
			}
			state=CLOCK_READY;
			TaskTimer.UpdateTask=1;
		}
		break;
    }
}
/**
  * @brief  时钟app的显示逻辑，由定时器定时触发
  * @param  无
  * @retval 无
  */
void CLOCK_Display(void)
{
	OLED_ClearBuf();               // 双缓冲清 0
    switch (state) 
	{
		case CLOCK_READY:
			
			
			// 检查是否有待处理的睡眠请求
			if(g_enter_sleep_pending) 
			{
				// 等待当前绘制完成（现在我们在低优先级中断中，不会被自己阻塞）
				if(g_is_drawing) {
					return;  // 如果正在绘制，下次中断再检查
				}
				
				// 等待DMA完成
				u32 t = 0;
				
				while(dma_done == 0 && ++t < 0x10000);
				
				// DMA超时处理（强制复位）
				if(t >= 0x10000) {
					
					
					//DMA_Cmd(DMA1_Channel6, DISABLE);
					//I2C_GenerateSTOP(I2C1, ENABLE);
					
					// I2C死锁恢复（时钟脉冲）
					GPIO_InitTypeDef GPIO_InitStruct;
					GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
					GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;
					GPIO_Init(GPIOB, &GPIO_InitStruct);
					for(int i = 0; i < 9; i++) {
						GPIO_SetBits(GPIOB, GPIO_Pin_6); Delay_us(5);
						GPIO_ResetBits(GPIOB, GPIO_Pin_6); Delay_us(5);
						
					}
					GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_OD;
					GPIO_Init(GPIOB, &GPIO_InitStruct);
					dma_done = 1;
					
					I2C_SoftwareResetCmd(I2C1, ENABLE);
					Delay_us(10);
					I2C_SoftwareResetCmd(I2C1, DISABLE);
					
				}
				//NVIC_SystemReset();
				// 现在安全熄屏
				//Delay_ms(100);  // 确保总线空闲
				//OLED_Refresh();
				OLED_I2C_Init();
				OLED_WriteCommand(0xAE);	
				state = CLOCK_SLEEP;		
				g_enter_sleep_pending = 0;	
				TaskTimer.UpdateTask = 0;  // 恢复定时器（或保持停止，视低功耗策略）
				
				return;  // 跳过后续绘制
			}
			
			Show_ClOCK_Ship();	//
			break;

    }
}

void Show_ClOCK_Ship(void)
{
	if(state != CLOCK_READY) return;
	g_is_drawing = 1;
	
	uint8_t hour, min, sec;
    int8_t offset;
    uint8_t idx;
    uint8_t x;
    
    // 读取RTC时间 
    MyRTC_ReadTime();
    hour = MyRTC_Time[3];
    min = MyRTC_Time[4];
    sec = MyRTC_Time[5];
    // 时间显示
    // 小时:分钟
	OLED_Printf(10, 48, 20, 12, 0, "%02d:%02d", hour, min);
	
    // 更新动画参数 
    anim_tick++;
    if(anim_tick >= 1) {  // 每？帧更新一次波浪（控制速度）
        wave_phase += 1;
        anim_tick = 0;
    }
    
    // 云朵缓慢左移 
    if(cloud1_x > 0) cloud1_x--; else cloud1_x = 140;
    if(cloud2_x > 0) cloud2_x--; else cloud2_x = 130;
    
    // 海鸥快速飞行（从右到左） 更新海鸥坐标
    if(bird_x > 0 && bird_y > 1) 
	{
        bird_x -= 2;
        bird_y += (sec % 2) ? 1 : -1;  // 上下起伏飞行
    } 
	else //超出区域重置坐标
	{
        bird_x = 140;  // 重置到右侧外
        bird_y = 12 + (sec % 10);      // 随机高度
    }
    
    // 1. 绘制云朵（顶部，棉花糖形状） 
    Draw_Cloud(cloud1_x, 8);
    Draw_Cloud(cloud2_x, 12);
    
    // 2. 绘制海鸥（V字形） 
    if(0 < bird_x && bird_x < 129) {
       Draw_Bird(bird_x,bird_y);	//
    }
    
    // 3. 绘制三层波浪（正弦曲线） 
    // Wave 1: 最上层，振幅3，速度1，相位wave_phase
    for(x = 0; x < 128; x++) {
        idx = (x + wave_phase) % 60;
        offset = (SIN_TAB[idx] * 3) / 100;
        OLED_DrawPixel(WAVE_Y1 + offset, x);
    }
    
    // Wave 2: 中层，振幅4，速度2（相位*2）
    for(x = 0; x < 128; x++) {
        idx = (x + wave_phase * 2) % 60;
        offset = (SIN_TAB[idx] * 4) / 100;
        OLED_DrawPixel(WAVE_Y2 + offset, x);
        // 加厚线条（每列画2个点）
        if(x % 2 == 0) OLED_DrawPixel(WAVE_Y2 + offset + 1, x);
    }
    
    // Wave 3: 最下层，振幅2，速度3，更密集
    for(x = 0; x < 128; x+=2) {  // 每2像素画一个点（稀疏效果）
        idx = (x + wave_phase * 3) % 60;
        offset = (SIN_TAB[idx] * 2) / 100;
        OLED_DrawPixel(WAVE_Y3 + offset, x);
        OLED_DrawPixel(WAVE_Y3 + offset, x+1);
    }
    
    // 4. 绘制帆船（随第二层波浪起伏） 
    // 计算船体应处的Y位置（基于SHIP_X和当前波浪相位）
	SHIP_X=-8+(sec % 60) * 2.27  ;//以60秒为周期，小船移动动画(sec % 60) * 2.27 
    idx = (SHIP_X + wave_phase * 2) % 60;	//获取当前相位
    int8_t ship_y = WAVE_Y2 + (SIN_TAB[idx] * 4) / 100 - 2;  // -2让船浮在波上
    
    Draw_Ship(SHIP_X, ship_y);//绘制帆船
    

	//
    
    /* 6. 秒数浮标灯（右下角闪烁） */
    if(sec % 2 == 0) {
        fillCircle(20, 120, 3);  // 实心圆点作为指示灯
    }
	
	g_is_drawing = 0;  // 标记绘制完成
}

/**
  * @brief  显示时钟布局,但不刷新屏幕,屏幕刷新交由定时器处理
  * @param  无
  * @retval 无
  */
void Show_ClOCK(void)
{

uint8_t hour, min, sec;
    int16_t x, y;
    uint8_t idx;
    
    /* 读取RTC时间 */
    MyRTC_ReadTime();
    hour = MyRTC_Time[3];
    min = MyRTC_Time[4];
    sec = MyRTC_Time[5];
    
    /* 1. 绘制雷达同心圆（3个圈） */
    OLED_DrawEllipse(RADAR_CX, RADAR_CY, RADAR_R1, RADAR_R1, 0);  // 内圈（分钟）
    OLED_DrawEllipse(RADAR_CX, RADAR_CY, RADAR_R2, RADAR_R2, 0);  // 中圈（小时）
    OLED_DrawEllipse(RADAR_CX, RADAR_CY, RADAR_R3, RADAR_R3, 0);  // 外圈（扫描范围）
    
    /* 2. 绘制十字坐标线 */
    Draw_Line(RADAR_CX - RADAR_R3, RADAR_CY, RADAR_CX + RADAR_R3, RADAR_CY);  // 横线
    Draw_Line(RADAR_CX, RADAR_CY - RADAR_R3, RADAR_CX, RADAR_CY + RADAR_R3);  // 竖线
    
    /* 3. 绘制目标点 - 小时（在中圈，r=20） */
    // 角度：每小时30度（360/12），索引 = hour * 5（因为30/6=5）
    idx = ((hour % 12) * 5 + min / 12) % 60;  // 加上分钟偏移使移动平滑
    Get_Radar_Pos(RADAR_R2, idx, &x, &y);
    OLED_FillRect(y - 1, x - 2, 4, 4, 0);  // 2x2实心方块表示目标
    
    /* 4. 绘制目标点 - 分钟（在内圈，r=10） */
    idx = min % 60;  // 每分钟6度，直接对应
    Get_Radar_Pos(RADAR_R1, idx, &x, &y);
    OLED_FillRect(y - 1, x - 2, 4, 4, 0);
    
    /* 5. 绘制扫描线（随秒转动，从中心到外圈） */
    idx = sec % 60;
    Get_Radar_Pos(RADAR_R3, idx, &x, &y);
    Draw_Line(RADAR_CX, RADAR_CY, x, y);
    
    /* 6. 右侧数据显示区域（分隔线 + 文字） */
    Draw_Line(75, 0, 75, 63);  // 分隔线
    
    OLED_Printf(80, 5, 8, 6, 0, "RADAR");      // 标题
    OLED_Printf(80, 20, 16, 8, 0, "%02d", hour);  // 小时
    OLED_Printf(80, 35, 16, 8, 0, "%02d", min);   // 分钟
    OLED_Printf(80, 50, 12, 6, 0, "%02d", sec);   // 秒（小字）

	
	/*	
	//时钟ui基础代码
	MyRTC_ReadTime();
	OLED_Printf(14,16,36,18,0,"%02d:%02d",MyRTC_Time[3],MyRTC_Time[4]);
	OLED_Printf(2,2,15,8,0,"%02d/%02d",MyRTC_Time[1],MyRTC_Time[2]);
	OLED_Printf(34,106,15,12,0,"%02d",MyRTC_Time[5]);
	*/
}


/**
  * @brief  极坐标转直角坐标（查表法，无浮点）
  * @param  r: 半径, angle_idx: 角度索引0-59（0=12点，顺时针）
  */
static void Get_Radar_Pos(uint8_t r, uint8_t angle_idx, int16_t *x, int16_t *y)
{
    // 将角度索引转换为查表索引（查表0度=3点钟，顺时针）
    // 12点钟对应查表索引15（cos=0, sin=100）
    uint8_t tab_idx = (60 + 15 - angle_idx) % 60;
    
    *x = RADAR_CX + (int16_t)(r * COS_TAB[tab_idx]) / 100;
    *y = RADAR_CY - (int16_t)(r * SIN_TAB[tab_idx]) / 100;  // 屏幕Y向下，取反
}

/**
  * @brief  Bresenham直线算法（使用OLED_DrawPixel）
  */
static void Draw_Line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int8_t dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int8_t dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int8_t err = dx + dy;
    
    while(1) {
		if(x0 >= 0 && x0 < 128 && y0 >= 0 && y0 < 64)
		{
			OLED_DrawPixel(y0, x0);  // 注意：OLED_DrawPixel参数是(Line, Column)即(y, x)
		}
        if(x0 == x1 && y0 == y1) break;
        int8_t e2 = 2 * err;
        if(e2 >= dy) { err += dy; x0 += sx; }
        if(e2 <= dx) { err += dx; y0 += sy; }
    }
}

static int8_t iabs(int8_t x) {
    return x < 0 ? -x : x;
}

/**
  * @brief  绘制云朵（像素艺术风格）
  */
static void Draw_Cloud(int8_t x, int8_t y)
{
	if(x > 125 || x < -10 || y < 1 || y > 62) return;  // y<1 防止 y-1 溢出
    //绘制三个部分为一个云朵
    // 中心团
    OLED_DrawPixel(y, x);
    OLED_DrawPixel(y, x+1);
    OLED_DrawPixel(y-1, x);    
    OLED_DrawPixel(y-1, x+1);
    OLED_DrawPixel(y+1, x);
    OLED_DrawPixel(y+1, x+1);
    
    // 左侧团
    if(x >= 2) {
        OLED_DrawPixel(y, x-2);
        OLED_DrawPixel(y-1, x-2);
    }
    
    // 右侧团
    if(x <= 124) {
        OLED_DrawPixel(y, x+3);
        OLED_DrawPixel(y-1, x+3);
        OLED_DrawPixel(y, x+4);
    }
}

/**
  * @brief  绘制海鸥（V字形）
  */
static void Draw_Bird(int8_t x, int8_t y)
{
    // 关键：y 必须 >=1 因为下面有 y-1，必须 <=62 避免越界
    if(x > 125 || x < 0 || y < 1 || y > 62) return;
    
    OLED_DrawPixel(y, x);
    OLED_DrawPixel(y-1, x+1);  // 现在 y-1 不会负数
    OLED_DrawPixel(y, x+2);
}

/**
  * @brief  绘制帆船（简化像素风格）
  */
static void Draw_Ship(int16_t x, int16_t y)
{
    // 船体（平底船）
    // 底部横线
    Draw_Line(x - 6, y, x + 6, y);
    // 两侧斜边
    Draw_Line(x - 6, y, x - 4, y - 3);
    Draw_Line(x + 6, y, x + 4, y - 3);
    // 船头船尾连接
    Draw_Line(x - 4, y - 3, x + 4, y - 3);
    
    // 桅杆（垂直线）
    Draw_Line(x, y - 3, x, y - 14);
    
    // 帆（直角三角形）
    Draw_Line(x, y - 14, x + 7, y - 5);  // 斜边
    Draw_Line(x + 7, y - 5, x, y - 5);    // 底边（水平）
    // 帆的骨架线
    Draw_Line(x + 2, y - 12, x + 5, y - 7);
}

/**
  * @brief  辅助：填充小圆（用于指示灯）
  */
static void fillCircle(uint8_t y, uint8_t x, uint8_t r)
{
    int8_t i, j;
    for(i = -r; i <= r; i++) {
        for(j = -r; j <= r; j++) {
            if(i*i + j*j <= r*r) {
                OLED_DrawPixel(y + i, x + j);
            }
        }
    }
}

//关闭不必要的GPIO
void GPIO_LowPower_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AIN; // 模拟输入，最低功耗
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;

    // 启用所有 GPIO 时钟（因为要配置它们）
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOB |
        RCC_APB2Periph_GPIOC |
        RCC_APB2Periph_GPIOD |
        RCC_APB2Periph_GPIOE, ENABLE);

    // === 配置 GPIOA: 全部设为 AIN（无保留引脚）===
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_All;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    // === 配置 GPIOB: 保留 PB0 和 PB11，其余设为 AIN ===
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_All & ~(GPIO_Pin_0 | GPIO_Pin_11);
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    // === 配置 GPIOC、GPIOD、GPIOE：全部设为 AIN ===
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_All;
    GPIO_Init(GPIOC, &GPIO_InitStruct);
    GPIO_Init(GPIOD, &GPIO_InitStruct);
    GPIO_Init(GPIOE, &GPIO_InitStruct);

    // === 可选：关闭未使用端口的时钟（但 PB 必须保持开启！）===
    // 注意：PB 不能关，因为 EXTI 唤醒需要
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOC |
        RCC_APB2Periph_GPIOD |
        RCC_APB2Periph_GPIOE, DISABLE);
    // GPIOB 和 AFIO 必须保持使能！
}

void System_ExitStopMode(void)
{
    // 1. 【最关键】重新配置系统时钟（）
	//SystemInit();
    SystemSET_SysClockConfig_HSI();
	
    // 2. 清除唤醒源的 EXTI 挂起位（必须！）
    EXTI_ClearITPendingBit(EXTI_Line0);   // PB0 唤醒
    EXTI_ClearITPendingBit(EXTI_Line11);  // PB11 唤醒

    // 4. 恢复硬件 I2C（推荐复位后重配，避免总线卡死）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    // 屏幕引脚 + I2C1 + DMA 初始化
    OLED_Init();
    //I2C_AcknowledgeConfig(I2C1, ENABLE); // 如果需要 ACK

	// 3. 恢复定时器（定时器2在使用）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
	
    // 5. 恢复其他外设（如 USART、SPI 等，按需添加）
    Encoder_Init();
	
	
    // USART_Init(USART1, &usart_cfg);
    // USART_Cmd(USART1, ENABLE);

    // 6. 【可选】恢复 GPIO（如果在进入 STOP 前禁用了它们）
    // 例如：恢复 LCD、LED、SPI 引脚等
    /*
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_Init(GPIOA, &gpio_lcd_cfg);
    GPIO_Init(GPIOB, &gpio_spi_cfg);
    */

    // 7. 如果使用了 systick，重新配置（因为时钟变了）
    if (SysTick_Config(SystemCoreClock / 1000)) {
        // 错误处理
    }

    // 至此，系统已完全恢复到 STOP 前状态
}
