/* ---------------- SelectorPhysics.h ---------------- */
#ifndef __SELECTOR_PHYSICS_H
#define __SELECTOR_PHYSICS_H

#include <stdint.h>

/* 坐标系：行Line(0~63)，列Column(0~127)，高Height，宽Width
 * 与OLED_ReverseArea参数顺序完全一致！
 */
typedef struct {
    int16_t line, column;           // 当前位置（行，列）
    int16_t height, width;          // 当前尺寸（高，宽）
    
    int16_t target_line, target_column; // 目标位置
    int16_t target_height, target_width; // 目标尺寸
    
    int16_t vel_line, vel_column;   // 移动速度
    int16_t vel_h, vel_w;           // 尺寸变化速度
    
    uint8_t stiffness;    // 弹簧硬度（15~25）
    uint8_t damping;      // 阻力系数（6~12）
    uint8_t is_moving;    // 是否正在动
} SelectorPhysics;

/* 三个函数：初始化、设目标、更新、绘制 */
void Selector_Init(SelectorPhysics *sel, int16_t line, int16_t column, int16_t height, int16_t width);
void Selector_SetTarget(SelectorPhysics *sel, int16_t line, int16_t column, int16_t height, int16_t width);
void Selector_Update(SelectorPhysics *sel);
void Selector_Draw(SelectorPhysics *sel);

#endif