/* ---------------- Animation.h ---------------- */

#ifndef __ANIMATION_H
#define __ANIMATION_H
#include <stdint.h>

typedef struct AnimationController{
	
	int16_t currentY;	//当前量Y,放大256倍缩小精度
	int16_t deltaY;		//差距量Y
	int16_t stepY;		//步长量Y
	
	int16_t currentX;
	int16_t deltaX;
	int16_t stepX;
	
	int16_t currentHeight;
	int16_t deltaHeight;
	int16_t stepHeight;
	
	int16_t currentWidth;
	int16_t deltaWidth;
	int16_t stepWidth;
	
} AnimationController;

extern AnimationController Controller[5];	//动画控制器

void Animation_Start(u8 Direction,AnimationController* Controller,u8 Frame);

typedef struct 
{
int16_t current_input;           // 当前位置
int16_t target_output; // 目标位置
int16_t velocity;   // 移动速度
uint8_t stiffness;    // 弹簧硬度（15~25）
uint8_t damping;      // 阻力系数（6~12）
uint8_t is_moving;    // 是否正在动
void (*action)(void);	//稳定后,执行该函数
}ElementOffset;	//离散二阶弹簧阻尼系统

void ElementOffset_Init(ElementOffset *sel, int16_t input); 
void ElementOffset_SetTarget(ElementOffset *sel, int16_t output,void (*action));
void ElementOffset_Update(ElementOffset *sel);
#endif