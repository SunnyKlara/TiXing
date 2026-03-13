/* ---------------- Snake.c ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include "OLED.h"
#include "Animation.h"
#include "MyTimer.h"
#include "Snake.h"
#include "Key.h"
#include "Delay.h"
#include "Action.h"
#include "MyRTC.h"
/*
编码器逆时针转:EVENT 1
编码器顺时针转:EVENT 2
按键中断1:EVENT 3
按键中断2:EVENT 4
*/
/* 按键事件值 */
#define ENC_CW      1
#define ENC_CCW     2
#define KEY_BACK    3
#define KEY_OK      4


/* 游戏参数 */
#define MAP_W       128
#define MAP_H       64
#define DOT_SIZE    6              // 4×4 像素一块
#define GRID_W      (MAP_W/DOT_SIZE)
#define GRID_H      (MAP_H/DOT_SIZE)
#define MAX_SNAKE   40

typedef enum {
    SNAKE_READY,
    SNAKE_PLAYING,
    SNAKE_PAUSE,
    SNAKE_OVER
} SNAKE_STATE;

/*---------- 全局变量 ----------*/
static SNAKE_STATE state = SNAKE_READY;

static struct {
    uint8_t x,y;
} snake[MAX_SNAKE];
static uint8_t snakeLen = 3;
static int8_t  dirX = 1, dirY = 0;   // 当前方向
static uint8_t foodX, foodY;
static uint8_t tickCnt = 0;          // 100 ms 进一次，想慢点就加计数器
static uint8_t rng_seed = 0x5A;

static uint8_t rand(void)
{
	MyRTC_ReadTime();
	rng_seed=MyRTC_Time[5]+MyRTC_Time[4];	//与实时时间有关的伪随机种子
    rng_seed = (rng_seed << 1) | ((rng_seed >> 7) & 1);
    rng_seed ^= 0x1B;
    return rng_seed;
}



/*---------- 内部函数 ----------*/
static void draw_block(uint8_t gx, uint8_t gy, uint8_t fill)
{
    uint8_t px = gx * DOT_SIZE;
    uint8_t py = gy * DOT_SIZE;
    OLED_FillRect(py, px, DOT_SIZE, DOT_SIZE, fill);
}
static void place_food(void)
{
    do {
        foodX = rand() % GRID_W;
        foodY = rand() % GRID_H;
    } while (draw_block(foodX, foodY, 2), 0); // 2=只检测，不画
}
static void snake_init(void)
{
    snakeLen = 3;
    for (uint8_t i = 0; i < snakeLen; i++) {
        snake[i].x = GRID_W/2 - i;
        snake[i].y = GRID_H/2;
    }
    dirX = 1; dirY = 0;
    place_food();
}
static void snake_move(void)
{
    /* 计算新头，允许穿墙 */
    int16_t nx = (int16_t)snake[0].x + dirX;
    int16_t ny = (int16_t)snake[0].y + dirY;

    /* 穿墙回绕 */
    if (nx < 0) nx = GRID_W - 1;
    else if (nx >= GRID_W) nx = 0;
    if (ny < 0) ny = GRID_H - 1;
    else if (ny >= GRID_H) ny = 0;

    /* 撞自己才死 */
    for (uint8_t i = 0; i < snakeLen; i++)
        if (snake[i].x == (uint8_t)nx && snake[i].y == (uint8_t)ny)
        { state = SNAKE_OVER; return; }

    /* 前移身体 */
    for (uint8_t i = snakeLen; i > 0; i--) snake[i] = snake[i-1];
    snake[0].x = (uint8_t)nx;
    snake[0].y = (uint8_t)ny;

    /* 吃食物 */
    if (snake[0].x == foodX && snake[0].y == foodY) {
        if (snakeLen < MAX_SNAKE) snakeLen++;
        place_food();
    }
}
/**
  * @brief  对外接口,初次进入应用调用本函数
  * @param  无
  * @retval 无
  */
void SNAKE_Enter(void)
{
	for(u8 i=3;i<12;i++)
	{
		OLED_OffsetUpdate(i,0);	//加速度
	}
	OLED_ClearBuf();
    state = SNAKE_READY;
	ActionDisplay_Init(SNAKE_Display);
	ActionKey_Init(SNAKE_KeyAction);
    snake_init();
    TaskTimer.UpdateTask = 1;
}

/**
  * @brief  应用的按键交互,,采用状态机
  * @param  无
  * @retval 无
  */
void SNAKE_KeyAction(uint8_t event)
{
    switch (state) {
    case SNAKE_READY:
        if (event == KEY_OK) { state = SNAKE_PLAYING; }
		else if (event == KEY_BACK) 
		{
			ACTION_Exit();
		}
        break;

	case SNAKE_PLAYING:
		if (event == ENC_CW) {              // 顺时针 → 蛇“右”转
			if (dirX == 1)  { dirX = 0; dirY = 1; }   // 原右→下
			else if (dirY == 1) { dirX = -1; dirY = 0; } // 原下→左
			else if (dirX == -1){ dirX = 0; dirY = -1;} // 原左→上
			else if (dirY == -1){ dirX = 1; dirY = 0; } // 原上→右
		}
		else if (event == ENC_CCW) {        // 逆时针 → 蛇“左”转
			if (dirX == 1)  { dirX = 0; dirY = -1; }  // 原右→上
			else if (dirY == -1){ dirX = -1; dirY = 0;} // 原上→左
			else if (dirX == -1){ dirX = 0; dirY = 1; } // 原左→下
			else if (dirY == 1) { dirX = 1; dirY = 0; } // 原下→右
		}
		else if (event == KEY_OK || event == KEY_BACK) {
			state = SNAKE_PAUSE;   // 两个按键都当暂停
		}
		break;

    case SNAKE_PAUSE:
        if (event == KEY_OK) { state = SNAKE_PLAYING; }
		else if (event == KEY_BACK) 
		{
			ActionKey_Free(SNAKE_KeyAction);
			ACTION_Exit();
		}
        break;

    case SNAKE_OVER:
        if (event == KEY_OK) { snake_init(); state = SNAKE_READY; }
		else if (event == KEY_BACK) 
		{
			ActionKey_Free(SNAKE_KeyAction);
			ACTION_Exit();
		}
        break;
    }
}
/**
  * @brief  应用的显示逻辑,由定时器调用
  * @param  无
  * @retval 无
  */
void SNAKE_Display(void)
{
    
	OLED_ClearBuf();               // 双缓冲清 0
    switch (state) {
    case SNAKE_READY:
        OLED_ShowString(30, 20, "SNAKE READY", 1);
        OLED_ShowString(10, 40, "OK:START", 1);
        break;

    case SNAKE_PLAYING:
		
        /* 画食物 */
        draw_block(foodX, foodY, 0);
        /* 画蛇 */
        for (uint8_t i = 0; i < snakeLen; i++)
            draw_block(snake[i].x, snake[i].y, 0);
        /* 每 100 ms 动一格 */
        snake_move();
        break;

    case SNAKE_PAUSE:
        OLED_ShowString(30, 25, "PAUSE", 1);
        OLED_ShowString(10, 30, "OK:CONTINUE", 1);
        break;

    case SNAKE_OVER:
        OLED_ShowString(30, 25, "GAME OVER", 1);
        OLED_ShowNum(50, 40, snakeLen, 3,16,8,1);
        OLED_ShowString(0, 15, "OK:RESTART", 1);
        break;
    }

}
