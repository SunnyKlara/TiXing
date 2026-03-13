/* ---------------- Action.h ---------------- */
/* ---------------- 菜单进入应用层的公共接口,提供了菜单进入应用的Enter函数 ---------------- */

#ifndef __ACTION_H
#define __ACTION_H
/* ---------------- 下方导入可供应用调用的公共函数 ---------------- */
void ActionKey_Init(void (*KeyAction)(uint8_t));
void ActionKey_Free(void (*KeyAction)(uint8_t));
void ActionDisplay_Init(void (*Display)(void));
void ACTION_Exit(void);			//统一退出接口
void Power_Init(void);
void StopMode_Enter(void);
/* ---------------- 下方导入应用的函数接口 ---------------- */
#include "TIMESET.h"
#include "FPSSET.h"
#include "MPU6050.h"
#include "Snake.h"
#include "PlaneWar.h"
#include "DinoWar.h"
#include "Clock.h"
#include "Emoji.h"
#include "FlappyBird.h"
#include "SystemSET.h"
#include "Stopwatch.h"
#include "MPU6050.h"
#endif