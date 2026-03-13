/* ---------------- TIMESET.h ---------------- */

#ifndef __TIMESET_H
#define __TIMESET_H
void TIMESET_Enter();
void TIMESET_KeyAction(uint8_t Event);
void TIMESET_ShowUI(void);
typedef struct TimeElement{
    const uint8_t Inx;
    uint16_t *Value;
	const char *Str;
	//void (*Action)(void);
} TimeElement;

typedef struct Contorller
{
	uint8_t BaseOffset;	//元素位置偏移,基本单位16行
	uint8_t PointerOffset;	//父页的指针偏移和屏幕基本偏移在进入子页后用缓存变量暂存,在回到父页后再重新恢复指针参数
	uint8_t ElementHeight;
	int8_t OffsetLine;//行偏移,基本单位1行
} Contorller;

void LineOffset_Reset(void);
#endif