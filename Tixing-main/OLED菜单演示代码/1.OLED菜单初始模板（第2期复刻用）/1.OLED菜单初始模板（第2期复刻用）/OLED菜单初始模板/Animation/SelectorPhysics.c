/* ---------------- SelectorPhysics.c ---------------- */
/* ---------------- 二阶弹簧阻尼系统制作反色框动画 ---------------- */
#include "SelectorPhysics.h"
#include "OLED.h"
#include <stdlib.h>
#include <math.h>
#define PHYSICS_SCALE 10  // 固定点数缩放,将浮点运算转为整数运算

/* 初始化：只调用一次 */
void Selector_Init(SelectorPhysics *sel, int16_t line, int16_t column, int16_t height, int16_t width) {
    // 所有值放大10倍存成整数，STM32才能算
    sel->line = sel->target_line = line * PHYSICS_SCALE;
    sel->column = sel->target_column = column * PHYSICS_SCALE;
    sel->height = sel->target_height = height * PHYSICS_SCALE;
    sel->width = sel->target_width = width * PHYSICS_SCALE;
    
    sel->vel_line = sel->vel_column = 0;
    sel->vel_h = sel->vel_w = 0;
    
    sel->stiffness = 30;  // 弹簧
    sel->damping = 4;     // 阻尼系数
    sel->is_moving = 0;   // 初始停止
}

/* 设新目标：每次切换选项时调用 */
//将系统的参考点（目标位置）从当前值改变到新值，导致输入偏差从0变为不为0，系统开始响应这个新的参考点。
void Selector_SetTarget(SelectorPhysics *sel, int16_t line, int16_t column, int16_t height, int16_t width) {
    sel->target_line = line * PHYSICS_SCALE;
    sel->target_column = column * PHYSICS_SCALE;
    sel->target_height = height * PHYSICS_SCALE;
    sel->target_width = width * PHYSICS_SCALE;
    sel->is_moving = 1;   // 标记开始运动
}

/* 更新输出量状态：在每帧调用 */
void Selector_Update(SelectorPhysics *sel) {
   if (!sel->is_moving) return;
   
   // 1. 计算时间步长（10ms = 0.01s）
   const float dt = 0.01f; // 10ms = 0.01秒
   
   // 2. 计算弹簧力（力 = -k * 位移）
   float spring_l = (sel->target_line - sel->line) * sel->stiffness;
    float spring_c = (sel->target_column - sel->column) * sel->stiffness;
    float spring_h = (sel->target_height - sel->height) * sel->stiffness;
    float spring_w = (sel->target_width - sel->width) * sel->stiffness;
    
    // 3. 计算阻尼力（力 = -d * 速度）
    float damp_l = sel->vel_line * sel->damping;
    float damp_c = sel->vel_column * sel->damping;
    float damp_h = sel->vel_h * sel->damping;
    float damp_w = sel->vel_w * sel->damping;
    
    // 4. 更新速度（速度变化 = 力/质量 * dt）
    // 这里质量设为1（简化计算）
    sel->vel_line += (spring_l - damp_l) * dt;
    sel->vel_column += (spring_c - damp_c) * dt;
    sel->vel_h += (spring_h - damp_h) * dt;
    sel->vel_w += (spring_w - damp_w) * dt;
    
    // 5. 更新位置
    sel->line += sel->vel_line * dt;
    sel->column += sel->vel_column * dt;
    sel->height += sel->vel_h * dt;
    sel->width += sel->vel_w * dt;
    
    // 6. 判断是否到达目标（误差<0.5像素）
    float dist_l = fabsf(sel->target_line - sel->line);
    float dist_c = fabsf(sel->target_column - sel->column);
    float dist_h = fabsf(sel->target_height - sel->height);
    float dist_w = fabsf(sel->target_width - sel->width);
    
	// 停止运动
    if (dist_l < 0.5f && dist_c < 0.5f && dist_h < 0.5f && dist_w < 0.5f) {
        sel->line = sel->target_line;
        sel->column = sel->target_column;
        sel->height = sel->target_height;
        sel->width = sel->target_width;
        sel->is_moving = 0;
    }
}

/* 绘制：把计算结果画到屏幕上 */
void Selector_Draw(SelectorPhysics *sel) {
    // 把放大10倍的值还原为真实像素
    int16_t draw_line = (sel->line + PHYSICS_SCALE/2) / PHYSICS_SCALE;
    int16_t draw_col = (sel->column + PHYSICS_SCALE/2) / PHYSICS_SCALE;
    int16_t draw_h = (sel->height + PHYSICS_SCALE/2) / PHYSICS_SCALE;
    int16_t draw_w = (sel->width + PHYSICS_SCALE/2) / PHYSICS_SCALE;
    
    /* 绝对安全限制：防止负数和溢出 */
    if (draw_h <= 0 || draw_w <= 0) return;  // 尺寸为0就不画
    
    if (draw_h > 64) draw_h = 64;
    if (draw_w > 128) draw_w = 128;
    
    if (draw_line < 0) {
        draw_h += draw_line;
        draw_line = 0;
    }
    if (draw_col < 0) {
        draw_w += draw_col;
        draw_col = 0;
    }
    
    if (draw_line + draw_h > 64) draw_h = 64 - draw_line;
    if (draw_col + draw_w > 128) draw_w = 128 - draw_col;
    
    // 最终检查：必须大于0
    if (draw_h > 0 && draw_w > 0) {
        OLED_ReverseArea(draw_line, draw_col, draw_h, draw_w);
    }
}

/*
	m·x'' + c·x' + k·x = F(t)
	质量m=1（简化计算），外力F(t)=0，动画系统中，我们不需要外力，因为目标位置就是系统的"平衡点",所以方程简化为：
	x'' = -k·x - c·x'
	
	刚度stiffness = 20 (k)：弹簧刚度系数
	阻尼系数damping = 5 (c)：阻尼系数
	dt = 0.01 (10ms)：时间步长
	
	ζ = c/(2·√(m·k)) = 5/(2·√20) ≈ 5/8.94 ≈ 0.56

	我们将方框视为一个系统，在某一时刻其处于位置1，现在在另一时刻我们想让其处于位置2，
		那么，位置2便是我们整个系统的期望输入，而系统的输出就是方框的实际位置

*/