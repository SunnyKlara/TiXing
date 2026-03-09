#ifndef _xuanniu_H_
#define _xuanniu_H_

#include "main.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "lcd.h"
#include "ws2812.h"
#include "w25q128.h"
#include "vs1003.h"
void LCD(void);
void Encoder(void);
void Encoder_Init(void);      // 编码器初始化（运行时重新配置TIM1）
int16_t Encoder_GetDelta(void); // 获取编码器旋转增量
void PWM(void);
void deng(void);
void deng_init(void);
void Throttle_ResetState(void);  // ✅ 新增：重置油门模式的静态变量

// ✅ LED平滑渐变函数声明
void LED_StartGradient(uint8_t led_index, uint8_t target_r, uint8_t target_g, uint8_t target_b, uint16_t speed_mode);
void LED_GradientProcess(void);
uint8_t LED_IsGradientActive(void);
void LED_GradientInit(void);
void Streamlight_Process(void);  // ✅ 流水灯效果处理
void Breath_Process(void);       // ✅ 呼吸灯效果处理（UI4外持续运行）

// 渐变速度模式常量
#define GRADIENT_SPEED_FAST   25   // 快速：0.5秒
#define GRADIENT_SPEED_NORMAL 75   // 正常：1.5秒
#define GRADIENT_SPEED_SLOW   150  // 慢速：3秒

// ✅ 菜单界面重构：新增全局变量声明
extern uint8_t menu_selected;   // 菜单选中的 UI（1-4）
extern int16_t encoder_delta;   // 编码器增量（统一读取）
extern uint8_t auto_enter;      // 蓝牙命令自动进入标志

// ═══════════════════════════════════════════════════════════════════
// 🎨 UI3 RGB调色界面状态机变量
// ═══════════════════════════════════════════════════════════════════
extern uint8_t ui3_mode;        // UI3模式：0=选灯带, 1=调RGB
extern uint8_t ui3_channel;     // RGB通道：0=R, 1=G, 2=B

extern uint8_t chu;
extern uint8_t red4,red2,red3,red1,green4,green2,green3,green1,blue4,blue2,blue3,blue1;
extern uint8_t WriteRead[16];  // 扩展为16字节，新增音量存储
#define jdt_hight          25
#define ui_zi_x            100
#define ui_zi_y            40

#define one_zi_x           47
#define two_zi_x           43
#define thr_zi_x           39


#define red_zi_y           82
#define green_zi_y         122
#define blue_zi_y          162


//红,255，0，0
//橙红204，119，34
//蓝33, 126, 222
//青0，234，255
//绿19，136，3
//紫75, 0, 130


#define qiu_col            0xD9A1
#define qiu_color          0xD6DA
#define qiu_r              4
#define qiu_1_x            120
#define qiu_1_y            105
#define qiu_2_x            130
#define qiu_2_y            110
#define qiu_3_x            135
#define qiu_3_y            120
#define qiu_4_x            130
#define qiu_4_y            130
#define qiu_5_x            120
#define qiu_5_y            135
#define qiu_6_x            110
#define qiu_6_y            130
#define qiu_7_x            105
#define qiu_7_y            120
#define qiu_8_x            110
#define qiu_8_y            110

#define yinying            0xE73C
#define yinying_r          18
#define jdt_yinying_x      70
#define jdt_yinying_width  127
#define deng_qiu_1_x       50
#define deng_qiu_1_y       90
#define deng_qiu_2_x       50
#define deng_qiu_2_y       130
#define deng_qiu_3_x       50
#define deng_qiu_3_y       170

#define tiao_1_x           132
#define tiao_1_y    	   90
#define tiao_2_x     	   132
#define tiao_2_y     	   130
#define tiao_3_x      	   132
#define tiao_3_y      	   170
#define tiao_di_width      127
#define jdt_yinying_1_y    106
#define jdt_yinying_2_y    146
#define jdt_yinying_3_y    186
#define jdt_yinying_hight  5

#define red_di             4
#define red_tiao           1
#define green_di           5
#define green_tiao         2
#define blue_di            6
#define blue_tiao          3
#define yinying_tiao       7
#define bright_tiao        8
#define bright_di_tiao     9

#define red_di_color             0xFDD7
#define red_tiao_color           0xF8A3
#define green_di_color           0xBEF2
#define green_tiao_color         0x05E0
#define blue_di_color            0x65DF
#define blue_tiao_color          0x1B5F

#define Main               1
#define left               2
#define right              3
#define tail               4

#define bright_x           120
#define bright_y           120
#define bright_width       200
#define bright_hight       34
#define bright_piancha     7
#define bright_bai         0xE71C
#define bright_di          0x630C
#define bright_num         0.01f
#endif
