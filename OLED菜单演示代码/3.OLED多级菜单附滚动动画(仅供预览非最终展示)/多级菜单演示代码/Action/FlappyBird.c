/* ---------------- FlappyBird.c ---------------- */
/* ---------------- 像素鸟游戏 ---------------- */
#include "stm32f10x.h"
#include "string.h"
#include "OLED.h"
#include "MyTimer.h"
#include "FlappyBird.h"
#include "Key.h"
#include "Action.h"
#include "Image.h"	

#define ENC_CCW     2
#define ENC_CW      1
#define KEY_OK      4
#define KEY_BACK    3

#define DOT_SIZE    5
#define GRID_W      (128 / DOT_SIZE)   // 32
#define GRID_H      (64 / DOT_SIZE)    // 16

typedef enum { FB_READY, FB_PLAYING, FB_OVER } FB_STATE;

static FB_STATE state = FB_READY;
static int Score = 0;
static uint8_t birdGridY = GRID_H / 2;   // 网格：0-15
static int8_t birdVY = 0;                // 有符号速度
static int8_t pipeCol = GRID_W - 1;     // 管道列：0-31
static uint8_t gapCenter = GRID_H / 2;   // 缺口中心（网格）

static uint8_t rng_seed = 0x5A;
static uint8_t rand(void){
    rng_seed = (rng_seed << 1) | ((rng_seed >> 7) & 1);
    rng_seed ^= 0x1B;
    return rng_seed;
}
const uint8_t Bird[]=
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x80,0x80,0x00,
	0x00,0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x3C,0x3C,0x7C,0xFC,0xFF,0xFE,0xFF,0xEF,0xFF,0xFF,0xBF,
	0x1F,0xDF,0xDF,0xFF,0xFB,0xFE,0xD4,0x40,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
	0x00,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};


static void draw_block(uint8_t gx, uint8_t gy, uint8_t fill){
    uint8_t px = gx * DOT_SIZE;
    uint8_t py = gy * DOT_SIZE;
    OLED_FillRect(py, px, DOT_SIZE, DOT_SIZE, 0); // 注意：必须写成了 0!!!否则无法运行
}

/* ---------- 鸟更新：更慢下坠 ---------- */
u8 Division=0;
static void bird_update(void)
{
	Division++;
	if(Division > 2)
	{
		birdVY += 1;
		if (birdVY > 2) birdVY = 2;  // 最大下落速度从 4 → 2

		int16_t newY = (int16_t)birdGridY + birdVY;
		if (newY < 0) {
			newY = 0;
			birdVY = 0;
		}
		if (newY >= GRID_H) {
			state = FB_OVER;
			return;
		}
		birdGridY = (uint8_t)newY;
		Division=0;
	}
}

/* ---------- 管道更新：保持逻辑，但确保缺口更大 ---------- */
static void pipe_update(void)
{
    if (pipeCol == 0)
    {
        pipeCol = GRID_W - 0;
        // 缺口中心范围：至少留出 3 格上下边距（0~15，所以 3~12）
        gapCenter = 3 + (rand() % (GRID_H - 6)); // 3 ~ 12
        Score++;
    }
    else {
        pipeCol--;
    }
}

/* ---------- 碰撞检测：6格高的缺口 ---------- */
static uint8_t isColliding(void)
{
    if (pipeCol != 4) return 0;

    uint8_t gapTop = gapCenter - 3;   // 上边界（包含）
    uint8_t gapBtm = gapCenter + 2;   // 下边界（包含）
    // 共 6 行：gapTop, +1, +2, +3, +4, +5 → 实际是 (gapBtm - gapTop + 1) = 6
	
    return (birdGridY < gapTop || birdGridY > gapBtm);
}

void FlappyBird_Init(void)
{
	 Score = 0;
    birdGridY = GRID_H / 2;
    birdVY = 0;
    pipeCol = GRID_W - 1;
    gapCenter = GRID_H / 2;
    state = FB_READY;
}

/* ========== 对外接口 ========== */
void FlappyBird_Enter(void)
{

	ActionDisplay_Init(FlappyBird_Display);
	ActionKey_Init(FlappyBird_KeyAction);
    TaskTimer.UpdateTask = 1;
	Timer_SetPrescaler((300*menuOptions.SystemClock/72));
	FlappyBird_Init();
}

void FlappyBird_KeyAction(uint8_t event)
{
    switch (state)
	{
		case FB_READY:
			if (event == KEY_OK) state = FB_PLAYING;
			if (event == KEY_BACK)
			{
				ACTION_Exit();
				Timer_SetPrescaler(700);
				break;
			}
			break;
		case FB_PLAYING:
			if (event == KEY_OK || event == ENC_CCW||event == ENC_CW ){
				birdVY = -3; // 跳跃力度稍弱，配合慢速下落
			}
			break;
		case FB_OVER:
			if (event == KEY_OK){
				FlappyBird_Init();
			}
			if (event == KEY_BACK)
			{
				
				ACTION_Exit();
				Timer_SetPrescaler(700);
				break;
			}
	}
}

void FlappyBird_Display(void)
{
    OLED_ClearBuf();

    switch (state){
    case FB_READY:
        OLED_ShowString(10, 40, "OK:START", 0);
		OLED_ShowString(30, 20, "FlappyBird", 0);
        break;

    case FB_PLAYING:
        OLED_ShowNum(0, 0, Score, 3, 16, 8, 0);

        /* 鸟 */
        //draw_block(4, birdGridY, 1);
		OLED_ShowImage(birdGridY*DOT_SIZE-2,1,24,32,Bird,1);
        /* 管道 */
        uint8_t gapTop = gapCenter - 3;
        uint8_t gapBtm = gapCenter + 2;

        for (uint8_t i = 0; i < gapTop; i++) {
            draw_block(pipeCol, i, 1);
        }
        for (uint8_t i = gapBtm + 1; i < GRID_H; i++) {
            draw_block(pipeCol, i, 1);
        }

        bird_update();
        pipe_update();
        if (isColliding()) {
            state = FB_OVER;
        }
        break;

    case FB_OVER:
        OLED_ShowString(30, 25, "GAME OVER", 0);
        OLED_ShowNum(50, 40, Score, 3, 16, 8, 0);
        OLED_ShowString(10, 55, "OK:RESTART", 0);
        break;
    }

}