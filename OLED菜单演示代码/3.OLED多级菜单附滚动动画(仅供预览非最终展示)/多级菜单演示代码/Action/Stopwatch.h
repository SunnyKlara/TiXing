/* ---------------- Stopwatch.h ---------------- */

#ifndef __Stopwatch_H
#define __Stopwatch_H

typedef struct STOPPWATCHTIME {
    uint8_t Hour;
    uint8_t Min;
    uint8_t Sec;
	uint8_t Dot;
} STOPPWATCHTIME;

void Stopwatch_KeyAction(uint8_t Event);
void Stopwatch_Enter(void);
void Stopwatch_Display(void);	//
void Show_Stopwatch(void);
void Stopwatch_Reset(void);
void Stopwatch_Exit(void);
#endif