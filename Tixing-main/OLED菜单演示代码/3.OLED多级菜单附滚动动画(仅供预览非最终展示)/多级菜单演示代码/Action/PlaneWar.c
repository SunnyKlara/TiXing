/* ---------------- PlaneWar.c ---------------- */
#include "stm32f10x.h"
#include "string.h"
#include "OLED.h"
#include "MyTimer.h"
#include "PlaneWar.h"
#include "Key.h"
#include "Image.h"
#include "Action.h"
/* 按键事件值 */
#define ENC_CCW     2
#define ENC_CW      1
#define KEY_OK      4
#define KEY_BACK    3

/* 游戏参数 */
#define MAP_W       128
#define MAP_H       64
#define DOT_SIZE    2
#define GRID_W      (MAP_W/DOT_SIZE)
#define GRID_H      (MAP_H/DOT_SIZE)

#define PLAYER_BUL_MAX      8
#define ENEMY_MAX           4
#define ENEMY_BUL_MAX       12
#define ENEMY_SPAWN_TICK    5        /* 500 ms 一架 */
#define PLAYER_FIRE_TICK    3        /* 300 ms 自动连发 */
#define ENEMY_FIRE_PROB     20        /* 1/8 概率射击 */

typedef enum { PW_READY, PW_PLAYING, PW_PAUSE, PW_OVER } PW_STATE;
typedef struct { uint8_t x, y, alive; } OBJ_T;

/*---------- 全局对象 ----------*/
static PW_STATE state     = PW_READY;
static OBJ_T    player    = { GRID_W/2, GRID_H-2, 1};
static OBJ_T    playerBul[PLAYER_BUL_MAX] = {0};
static OBJ_T    enemies [ENEMY_MAX]       = {0};
static OBJ_T    enemyBul[ENEMY_BUL_MAX]   = {0};
static uint8_t  score = 0;
static uint8_t  tick  = 0;          /* 100 ms 计数 */
static uint8_t  playerFireTick = 0;

/*---------- 飞机 8×8 图标 ----------*/
static const uint8_t player_plane[8] = {
    0x44,0x26,0x1E,0x1E,0x26,0x44
};
static const uint8_t enemy_plane[8] = {
    0x44,0x26,0x1E,0x1E,0x26,0x44
};

/*---------- 工具 ----------*/
static uint8_t rng_seed = 0x5A;
static uint8_t rand(void){
    rng_seed = (rng_seed<<1)|((rng_seed>>7)&1);
    rng_seed ^= 0x1B;
    return rng_seed;
}
static void draw_block(uint8_t gx, uint8_t gy, uint8_t fill){
    uint8_t px = gx * DOT_SIZE;
    uint8_t py = gy * DOT_SIZE;
    OLED_FillRect(py, px, DOT_SIZE, DOT_SIZE, fill);
}
/* 画 6×6 图标（左上角锚点 gx,gy） */
static void draw_plane(uint8_t gx, uint8_t gy, const uint8_t *ico)
{
    for (uint8_t col = 0; col < 6; col++)          /* 先列后行 */
    {
        uint8_t colData = ico[col];                /* 1 字节 = 1 列 6 点 */
        for (uint8_t bit = 0; bit < 6; bit++)      /* 从低到高 = 从上到下 */
        {
            if (colData & (1 << bit))
                draw_block(gx + col, gy + bit, 0); /* 原样列坐标 */
        }
    }
}

/*---------- 子弹更新 ----------*/
static void bullet_update(OBJ_T *bul, uint8_t max, int8_t dir){
    for (uint8_t i = 0; i < max; i++){
        if (!bul[i].alive) continue;
        int8_t ny = (int8_t)bul[i].y + dir;
        if (ny < 0 || ny >= GRID_H) { bul[i].alive = 0; continue; }
        bul[i].y = (uint8_t)ny;
    }
}

/*---------- 矩形碰撞（中心 6×6） ----------*/
static uint8_t collision(OBJ_T *bul, uint8_t bulMax, OBJ_T *tgt, uint8_t tgtMax){
    for (uint8_t i = 0; i < bulMax; i++){
        if (!bul[i].alive) continue;
        /* 子弹中心 1×1 → 与目标 3×3 区域相交即命中 */
        int8_t bx = bul[i].x;
        int8_t by = bul[i].y;
        for (uint8_t j = 0; j < tgtMax; j++){
            if (!tgt[j].alive) continue;
            /* 目标左上角 = (x-1,y-1)  3×3 区域 */
            int8_t tx0 = tgt[j].x - 1;
            int8_t tx1 = tgt[j].x + 1;
            int8_t ty0 = tgt[j].y - 1;
            int8_t ty1 = tgt[j].y + 1;
            if (bx >= tx0 && bx <= tx1 && by >= ty0 && by <= ty1){
                bul[i].alive = 0;
                tgt[j].alive = 0;
                return 1;
            }
        }
    }
    return 0;
}

/*---------- 敌机生成 ----------*/
static void enemy_spawn(void){
    if (++tick < ENEMY_SPAWN_TICK) return;
    tick = 0;
    for (uint8_t i = 0; i < ENEMY_MAX; i++){
        if (enemies[i].alive) continue;
        enemies[i].x = rand() % GRID_W;
        enemies[i].y = 0;
        enemies[i].alive = 1;
        break;
    }
}

/*---------- 敌机移动 + 射击 ----------*/
static void enemy_action(void){
    static uint8_t enemyMoveTick = 0;                   /* 独立慢速计数 */
    for (uint8_t i = 0; i < ENEMY_MAX; i++){
        if (!enemies[i].alive) continue;

        /* ① 射击（1/8 概率，每帧判断） */
        if ((rand() & 0x07) == 0){
            for (uint8_t k = 0; k < ENEMY_BUL_MAX; k++){
                if (enemyBul[k].alive) continue;
                enemyBul[k].x = enemies[i].x;
                enemyBul[k].y = enemies[i].y + 1;
                enemyBul[k].alive = 1;
                break;
            }
        }

        /* ② 慢速向下：1 格 / 2 帧 */
        if (++enemyMoveTick >= 2){
            enemyMoveTick = 0;
            if (enemies[i].y >= GRID_H - 1) enemies[i].alive = 0;
            else enemies[i].y++;
        }
    }
}

/*---------- 玩家自动射击 ----------*/
static void player_auto_fire(void){
    if (++playerFireTick < PLAYER_FIRE_TICK) return;
    playerFireTick = 0;
    for (uint8_t i = 0; i < PLAYER_BUL_MAX; i++){
        if (playerBul[i].alive) continue;
        playerBul[i].x = player.x;
        playerBul[i].y = player.y - 1;
        playerBul[i].alive = 1;
        break;
    }
}

/*---------- 游戏初始化 ----------*/
static void game_init(void){
    score = 0; tick = 0; playerFireTick = 0;
    player.x = GRID_W / 2; player.y = GRID_H - 2; player.alive = 1;
    memset(playerBul, 0, sizeof(playerBul));
    memset(enemies, 0, sizeof(enemies));
    memset(enemyBul, 0, sizeof(enemyBul));
    rng_seed ++;
}

/*========== 对外接口 ==========*/
void PlaneWar_Enter(void){
    state = PW_READY;
    game_init();
	ActionKey_Init(PlaneWar_KeyAction);
	ActionDisplay_Init(PlaneWar_Display);
    TaskTimer.UpdateTask = 1;
	Timer_SetPrescaler((300*menuOptions.SystemClock/72));
}

void PlaneWar_KeyAction(uint8_t event){
    switch (state){
    case PW_READY:
        if (event == KEY_OK) state = PW_PLAYING;
		else if (event == KEY_BACK) 
		{
			ACTION_Exit();
		}
        break;
    case PW_PLAYING:
        if (event == ENC_CCW && player.x > 0) player.x--;
        if (event == ENC_CW  && player.x < GRID_W - 1) player.x++;
        if (event == KEY_OK || event == KEY_BACK) state = PW_PAUSE;
        break;
    case PW_PAUSE:
        if (event == KEY_OK) state = PW_PLAYING;
		else if (event == KEY_BACK) 
		{
			ActionKey_Free(PlaneWar_KeyAction);
			ACTION_Exit();
			Timer_SetPrescaler(720);
		}
        break;
    case PW_OVER:
        if (event == KEY_OK) { game_init(); state = PW_READY; }
		else if (event == KEY_BACK) 
		{
			ActionKey_Free(PlaneWar_KeyAction);
			ACTION_Exit();
			Timer_SetPrescaler(720);
		}
        break;
    }
}

/**
  * @brief  应用的显示逻辑,由定时器调用
  * @param  无
  * @retval 无
  */
void PlaneWar_Display(void){
    OLED_ClearBuf();
    switch (state){
    case PW_READY:
        OLED_ShowString(30, 10, "PlaneWar READY", 0);
        OLED_ShowString(10, 30, "OK:START", 0);
        break;
    case PW_PLAYING:
        /* 画飞机 8×8 */
        draw_plane(player.x - 3, player.y -4, player_plane);
        for (uint8_t i = 0; i < ENEMY_MAX; i++){
            if (!enemies[i].alive) continue;
            draw_plane(enemies[i].x - 3, enemies[i].y -3, enemy_plane);
        }
        /* 画子弹（单点）*/
        for (uint8_t i = 0; i < PLAYER_BUL_MAX; i++)
            if (playerBul[i].alive) draw_block(playerBul[i].x, playerBul[i].y, 0);
        for (uint8_t i = 0; i < ENEMY_BUL_MAX; i++)
            if (enemyBul[i].alive) draw_block(enemyBul[i].x, enemyBul[i].y, 0);

        /* 逻辑更新 */
        player_auto_fire();
        bullet_update(playerBul, PLAYER_BUL_MAX, -1);   /* 玩家弹：快，1 格/帧 */
        bullet_update(enemyBul,  ENEMY_BUL_MAX,  1);   /* 敌弹：快，1 格/帧 */
        if (collision(playerBul, PLAYER_BUL_MAX, enemies, ENEMY_MAX)) score++;
        if (collision(enemyBul,  ENEMY_BUL_MAX,  &player, 1)) state = PW_OVER;
        enemy_action();          /* 敌机慢速 + 射击 */
        enemy_spawn();
        OLED_ShowNum(0, 0, score, 2, 16,8,0);
        break;
    case PW_PAUSE:
        OLED_ShowString(40, 25, "PAUSE", 0,16,8);
        OLED_ShowString(10, 20, "OK:CONTINUE", 0,16,8);
        break;
    case PW_OVER:
        OLED_ShowString(30, 25, "GAME OVER", 0,16,8);
        OLED_ShowNum(50, 40, score, 2,16,8, 0);
        OLED_ShowString(10, 50, "OK:RESTART", 0,16,8);
        break;
    }
}

