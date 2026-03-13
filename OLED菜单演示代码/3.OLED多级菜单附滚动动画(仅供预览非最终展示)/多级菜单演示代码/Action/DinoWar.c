/* ---------------- DinoWar.c ---------------- */
#include "stm32f10x.h"
#include "string.h"
#include "OLED.h"
#include "MyTimer.h"
#include "DinoWar.h"
#include "Key.h"
#include "DinoWar_Image.h"
#include <math.h>
#include "Image.h"
#include "Action.h"

/* 按键事件值（同框架） */
#define ENC_CCW     2
#define ENC_CW      1
#define KEY_OK      4
#define KEY_BACK    3

/* 游戏参数 */
#define DOT_SIZE    2
#define GRID_W      (128/DOT_SIZE)
#define GRID_H      (64/DOT_SIZE)

typedef enum { DINO_READY, DINO_PLAYING, DINO_OVER } DINO_STATE;

/* ---------- 全局对象 ---------- */
static DINO_STATE state = DINO_READY;
static int Score = 0;
static uint16_t Ground_Pos = 0;
static uint8_t Barrier_Pos = 0;
static uint8_t Cloud_Pos = 0;
static uint8_t dino_jump_flag = 0;
static uint16_t jump_t = 0;



static uint8_t rng_seed = 0xAA;
static uint8_t rand(void)
{
    rng_seed = (rng_seed << 1) | ((rng_seed >> 7) & 1);
    rng_seed ^= 0x1B;
    return rng_seed;
}

static u8 inx;
/* 碰撞框 */
typedef struct { uint8_t minX, minY, maxX, maxY; } Rect_t;
static Rect_t dinoBox, barrierBox;

/* ---------- 工具： ShowImage 封装 ---------- */
static inline void showImg(int16_t x, int16_t y, uint8_t w, uint8_t h, const uint8_t *img){
    OLED_ShowImage(y, x, h, w, img, 0);  /* 像素→行/列，Mode=0 覆盖 */
}

/* ---------- 地面：草地循环显示 ---------- */
static void drawGround(void){
    /* 每 1 帧滚动 2 像素 */
    static uint8_t groundTick = 0;
    if (++groundTick >= 1){
        groundTick = 0;
        Ground_Pos += 2;
        Barrier_Pos += 2;
        if (Barrier_Pos >= 144) 
		{
			inx=rand()%3;
			Barrier_Pos = 0;
		}
    }

    // 限制 Ground_Pos 在 0 ~ 127 范围内循环（草地宽度=128）
    uint16_t pos = Ground_Pos % 128;

    // 直接绘制整行草地到 OLED_BUF 第 7 页（y=56~63）
    for (uint8_t i = 0; i < 128; i++) {
        uint8_t src_idx = (pos + i) % 128;  // 循环取模
        OLED_BUF[buf_idx][7][i] = Ground[src_idx];
    }
}

/* ---------- 云朵 ---------- */
static void drawCloud(void){
    int16_t px = 127 - Cloud_Pos;
    int16_t py = 9;
    showImg(px, py, 16, 8, Cloud);   /* 原版坐标 */
}


/* ---------- 障碍物 ---------- */
static void drawBarrier(void){
	/*
    if (Barrier_Pos >= 144) 
	{
		Barrier_Pos = 0;
	}
	*/
    int16_t px = 127 - Barrier_Pos;
    int16_t py = 44;
    showImg(px, py, 16, 18, Barrier[inx]);   /* 原版坐标 */
    barrierBox.minX = px;
    barrierBox.maxX = px + 2;
    barrierBox.minY = py;
    barrierBox.maxY = py + 16;	//碰撞箱判定
}

/* ---------- 恐龙：像素坐标 + 跳跃 ---------- */
static void drawDino(void){
    int8_t Jump_Pos = 0;
    if (dino_jump_flag){
        Jump_Pos = (int8_t)(28.0f * sinf(3.14159f * jump_t / 1000.0f));
        jump_t += 20;
        if (jump_t >= 1000){ jump_t = 0; dino_jump_flag = 0; }
    }

    /* 原版坐标：0,44 或 0,44-Jump_Pos */
    if (!dino_jump_flag)
        showImg(0, 44, 16, 18, (Cloud_Pos & 1) ? Dino[1] : Dino[0]);
    else
        showImg(0, 44 - Jump_Pos, 16, 18, Dino[2]);

    dinoBox.minX = 0;
    dinoBox.maxX = 16;
    dinoBox.minY = 44 - Jump_Pos;
    dinoBox.maxY = 62 - Jump_Pos;
}

/* ---------- 得分---------- */
static void showScore(void){
    OLED_ShowNum(0, 0, Score, 5, 16, 8, 0);   /* 原版位置 */
}

/* ---------- 速度控制---------- */
static void dinoTick(void){
    static uint8_t scoreCnt = 0, cloudCnt = 0;

    scoreCnt++;
    cloudCnt++;

    if (scoreCnt >= 100){ scoreCnt = 0; Score++; }

    if (cloudCnt >= 10){
        cloudCnt = 0;
        Cloud_Pos++;
        if (Cloud_Pos >= 200) Cloud_Pos = 0;
    }
}

/* ---------- 碰撞检测 ---------- */
static uint8_t isColliding(void){
    return (dinoBox.maxX > barrierBox.minX) && (dinoBox.minX < barrierBox.maxX) &&
           (dinoBox.maxY > barrierBox.minY) && (dinoBox.minY < barrierBox.maxY);
}

void DinoWar_Init(void)
{
	Score = Ground_Pos = Barrier_Pos = Cloud_Pos = jump_t = dino_jump_flag = 0;
    state = DINO_READY;
}

/* ========== 对外接口 ========== */
void DinoWar_Enter(void)
{
	ActionDisplay_Init(DinoWar_Display);
	ActionKey_Init(DinoWar_KeyAction);
    TaskTimer.UpdateTask = 1;
	Timer_SetPrescaler(100*menuOptions.SystemClock/72);
	DinoWar_Init();
}

/* ---------- 按键事件：所有动作在这里执行 ---------- */
void DinoWar_KeyAction(uint8_t event)
{
    switch (state){
    case DINO_READY:
        if (event == KEY_OK) state = DINO_PLAYING;
		else if (event == KEY_BACK) 
		{
			ACTION_Exit();
		}
        break;
    case DINO_PLAYING:
		
        if (event == KEY_OK || event == ENC_CCW || event == ENC_CW) dino_jump_flag = 1;   /* 跳跃 */
        if (event == KEY_BACK) state = DINO_OVER;  /* 立即结束 */
        break;
    case DINO_OVER:
        if (event == KEY_OK) DinoWar_Init();      /* 再来一局 */
		if (event == KEY_BACK) 
		{
			ACTION_Exit();
			Timer_SetPrescaler(720);	//恢复定时器配置
		};  /* 立即结束 */
        break;
    }
}

/* ---------- 显示：只负责画面和逻辑 ---------- */
void DinoWar_Display(void)
{
    OLED_ClearBuf();

    switch (state){
    case DINO_READY:
        OLED_ShowString(30, 10, "DinoWar READY", 0);
        OLED_ShowString(10, 40, "OK:START", 0);
        break;

    case DINO_PLAYING:
        showScore();
        drawGround();   
        drawCloud();
        drawBarrier();
        drawDino();
        dinoTick();
        if (isColliding()){ state = DINO_OVER; }
        break;

    case DINO_OVER:
        OLED_ShowString(28, 24, "Game Over", 1);
        OLED_ShowNum(50, 40, Score, 5, 16, 8, 1);
        OLED_ShowString(10, 40, "OK:RESTART", 1);
        break;
    }

}