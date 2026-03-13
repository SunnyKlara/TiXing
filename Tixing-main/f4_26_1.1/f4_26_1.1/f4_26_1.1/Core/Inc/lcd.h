#ifndef _lcd_H_
#define _lcd_H_

#include "main.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "lcd_init.h"
#include "stdlib.h"
#include "string.h"
#include <stdbool.h> 
#include "math.h"
#include <stdint.h>
#include "xuanniu.h"
// 角度转弧度
#define DEG_TO_RAD(deg) ((deg) * 3.1415926f / 180.0f)  
void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color);
void LCD_DrawGradientBar(u16 x, u16 y, u16 width, u16 height, u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2);
void LCD_DrawSolidBar(u16 x, u16 y, u16 width, u16 height, u8 r, u8 g, u8 b);
void LCD_DrawPoint(u16 x,u16 y,u16 color);
void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2,u16 color);
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2,u16 color);
void Draw_Circle(u16 x0,u16 y0,u8 r,u16 color);

void LCD_ShowChinese(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);
void LCD_ShowChinese12x12(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);
void LCD_ShowChinese16x16(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);
void LCD_ShowChinese24x24(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);
void LCD_ShowChinese32x32(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);

void LCD_ShowChar(u16 x,u16 y,u8 num,u16 fc,u16 bc,u8 sizey,u8 mode);
void LCD_ShowString(u16 x,u16 y,const u8 *p,u16 fc,u16 bc,u8 sizey,u8 mode);
u32 mypow(u8 m,u8 n);
void LCD_ShowIntNum(u16 x,u16 y,u16 num,u8 len,u16 fc,u16 bc,u8 sizey);
void LCD_ShowFloatNum1(u16 x,u16 y,float num,u8 len,u16 fc,u16 bc,u8 sizey);

void LCD_ShowPicture(u16 x,u16 y,u16 length,u16 width,const u8 pic[]);
void LCD_picture(u16 num,u8 key_num);
void LCD_picture_animated(u16 num, u8 key_num, int8_t delta);  // ✅ 带跳跃动画的速度更新
void Fill_Circle(u16 x0, u16 y0, u8 r, u16 color);
void LCD_DrawRoundedBar(u16 x, u16 y, u16 width, u16 height, u16 color);
void LCD_ui2(void);
void LCD_ui4(void);
void LCD_ui3(void);
void LCD_ui1(void);
void LCD_ui0(void);
void lcd_zi(u8 num);
void lcd_wuhuaqi(u8 num,u8 key_num);
void lcd_pei_se(u8 num,u8 state);
void lcd_pei_se_force(u8 num, u8 state);  // ✅ 强制刷新颜色条
void lcd_pei_se_dot(u8 flow_state);       // ✅ 刷新流水灯状态红点
void lcd_pei_se_realtime(u8 r1, u8 g1, u8 b1, u8 r2, u8 g2, u8 b2);  // ✅ 实时颜色条
void lcd_rgb_num(u8 rgb,u8 num);
void lcd_name_update(u8 num);
void lcd_rgb_update(u8 num);
void lcd_deng(u8 key_num,u8 name);
void LCD_ui4_num_update(u16 num);
void lcd_ui4_deng(u8 key_num);
void lcd_rgb_cai_update(u8 num);

// ✅ UI4 呼吸灯颜色条函数
void LCD_DrawBreathBar(u8 brightness, u8 r, u8 g, u8 b);
void LCD_DrawBreathBarGradient(u8 bright_factor);
void LCD_ClearBreathBar(void);

// ✅ UI5蓝牙调试界面已删除
// ✅ UI6音量调节界面
void LCD_ui6(void);                    // 音量界面初始化
void LCD_ui6_num_update(u8 volume);    // 更新音量数字显示
void LCD_ui6_volume_bar(u8 volume);    // 更新音量条
void lcd_ui6_deng(u8 state);           // 更新状态指示灯

// ✅ 菜单界面重构：新增菜单相关函数声明
void LCD_Menu_Init(void);                           // 菜单界面初始化
void Draw_Menu_Items(void);                         // 绘制菜单项（兼容旧接口）
void Draw_Menu_Border(uint8_t item, uint8_t show);  // 绘制/清除边框（兼容旧接口）
void Draw_Menu_Page(uint8_t page);                  // ✅ 新增：绘制指定菜单页面

// 假设屏幕分辨率为 240x240（圆形屏适配），可根据实际修改
#define LCD_WIDTH    240
#define LCD_HEIGHT   240

// ✅ 菜单界面重构：菜单项位置、尺寸、颜色常量
#define MENU_ITEM_WIDTH   80
#define MENU_ITEM_HEIGHT  60
#define MENU_ITEM_1_X     30   // 速度
#define MENU_ITEM_1_Y     50
#define MENU_ITEM_2_X     130  // 预设
#define MENU_ITEM_2_Y     50
#define MENU_ITEM_3_X     30   // 调色
#define MENU_ITEM_3_Y     130
#define MENU_ITEM_4_X     130  // 亮度
#define MENU_ITEM_4_Y     130
#define MENU_BORDER_COLOR 0xFFE0  // 黄色边框
#define MENU_BG_COLOR     0x0000  // 黑色背景
#define MENU_TEXT_COLOR   0xFFFF  // 白色文字
#define MENU_BORDER_WIDTH 3       // 边框宽度
//void LCD_UpdateDashboard(u16 x0, u16 y0, u16 radius, u16 value);
//void LCD_DrawDashboardBackground(u16 x0, u16 y0, u16 radius);


#define jindutiao_width  127
//»­±ÊÑÕÉ«
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE           	 0x001F  
#define BRED             0XF81F//ç´«è‰²
#define GBLUE			       0X07FF//æµ…è“
#define RED           	 0xF800
#define GREEN         	 0x07E0//ç»¿è‰²
#define CYAN          	 0x7FFF//é’
#define YELLOW        	 0xFFE0////é»„è‰²
#define BROWN 			     0XBC40 //å±Ž
#define BRRED 			     0XFC07 //åœŸé»„
#define GRAY  			     0X8430 //ç°
#define GRAY1  			     0x2145 //é£Žæ´žç°
#define DARKBLUE      	 0X01CF	//æ·±è“
#define LIGHTBLUE      	 0X7D7C	//  å¾®ä¿¡æ‰¾å›¾ç‰‡
#define GRAYBLUE       	 0X5458 //
#define LIGHTGREEN     	 0X841F //
#define LGRAY 			     0XC618 //
#define LGRAYBLUE        0XA651 //
#define LBBLUE           0X2B12 //

#endif
