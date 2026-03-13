#ifndef __KEY_H
#define __KEY_H
void Key_Init(void);
#include "EventBus.h"
uint8_t Is_KeyPressed(EventType ev);
void KeyLogic_Process_Back(void);
void KeyLogic_Process_OK(void);
uint8_t Key_GetNum(void);

#endif