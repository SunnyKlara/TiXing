/**
  ******************************************************************************
  * @file    rx.c
  * @brief   蓝牙命令接收与解析模块
  * @note    协议格式: CMD:PARAM\n
  *          支持命令:
  *          - WUHUA:0/1  雾化器开关
  *          - FAN:0-100  风扇速度
  *          - LED:s:r:g:b  LED颜色 (s=1-4灯带, rgb=0-255)
  ******************************************************************************
  */

#include "rx.h"
#include "ws2812.h"
#include "xuanniu.h"
#include "engine_audio.h" // ✅ 必须包含，否则远程油门指令无法调用 EngineAudio_Start/Stop
#include "logo.h"         // 🆕 自定义Logo上传模块

// ╔══════════════════════════════════════════════════════════════╗
// ║          外部变量引用 (来自 xuanniu.c)                        ║
// ╚══════════════════════════════════════════════════════════════╝
extern u8 wuhuaqi_state, wuhuaqi_state_old;
extern u8 wuhuaqi_state_saved;
extern int16_t Num;
extern uint8_t speed_value;
extern u8 ui, chu;
extern int16_t bright;
extern int8_t name;
extern uint8_t deng_num;
extern u8 pei_se_state; // ✅ 补全声明

// ✅ 菜单界面重构：引用菜单相关变量
extern uint8_t menu_selected;
extern uint8_t auto_enter;

extern int16_t red1_zhong, green1_zhong, blue1_zhong;

// ✅ 精度修复：存储绝对速度值 (0-340 km/h)，避免 Num 映射精度损失
int16_t current_speed_kmh = 0;  // 当前绝对速度（由 APP 或硬件旋钮设置）
extern int16_t red2_zhong, green2_zhong, blue2_zhong;
extern int16_t red3_zhong, green3_zhong, blue3_zhong;
extern int16_t red4_zhong, green4_zhong, blue4_zhong;
extern uint8_t red1, red2, red3, red4;
extern uint8_t green1, green2, green3, green4;
extern uint8_t blue1, blue2, blue3, blue4;
extern volatile uint8_t preset_dirty; // 预设被远程修改标志位，用于进入界面时刷新LCD

// 外部函数引用
extern void lcd_rgb_num(u8 rgb, u8 num);
extern void lcd_rgb_update(u8 num);
extern void lcd_pei_se(u8 num, u8 state);
extern void LCD_ui4_num_update(u16 num); // ✅ 修正为 u16
extern void lcd_name_update(u8 num);
extern void deng_ui2(void);
extern void deng(void);

// ╔══════════════════════════════════════════════════════════════╗
// ║          接收缓冲区                                           ║
// ╚══════════════════════════════════════════════════════════════╝
uint8_t usart5_rx_buffer[USART5_BUFFER_SIZE];
uint16_t usart5_rx_len = 0;

uint16_t rx_pointer = 0;  // 🆕 改为 uint16_t 以支持更大的缓冲区
u8 rx_data = 0;
u8 rx_buff[RX_BUFFER_SIZE];
u32 rx_tick = 0;
uint32_t remote_active_tick = 0; // ✅ 记录远程指令活跃时间戳

// ╔══════════════════════════════════════════════════════════════╗
// ║          蓝牙命令解析函数                                     ║
// ║          协议格式: CMD:PARAM\n                               ║
// ╚══════════════════════════════════════════════════════════════╝
void BLE_ParseCommand(char* cmd)
{
    // 去除末尾的换行符和回车符
    int len = strlen(cmd);
    while(len > 0 && (cmd[len-1] == '\n' || cmd[len-1] == '\r')) {
        cmd[--len] = '\0';
    }
    
    if(len == 0) return;
    
    // 🔧 调试：只记录START命令，不记录DATA命令（避免阻塞蓝牙）
    if (strncmp(cmd, "LOGO_START", 10) == 0) {
        printf("[BLE] Logo START command (len=%d)\r\n", len);
    }
    
    // ════════════════════════════════════════════════════════════
    // 🌫️ 雾化器控制: WUHUA:0 或 WUHUA:1
    // ════════════════════════════════════════════════════════════
    if(strncmp(cmd, "WUHUA:", 6) == 0)
    {
        int state = atoi(cmd + 6);
        
        // 只在非油门模式下允许控制
        if(wuhuaqi_state != 2)
        {
            if(state == 1)
            {
                wuhuaqi_state = 1;
                wuhuaqi_state_old = 1;
                wuhuaqi_state_saved = 1;
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
                printf("[BLE] WUHUA ON\r\n");
            }
            else
            {
                wuhuaqi_state = 0;
                wuhuaqi_state_old = 0;
                wuhuaqi_state_saved = 0;
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
                printf("[BLE] WUHUA OFF\r\n");
            }
            
            // 如果当前在界面1，更新LCD显示
            if(ui == 1)
            {
                lcd_wuhuaqi(wuhuaqi_state, speed_value);
            }
        }
        else
        {
            printf("[BLE] WUHUA ignored (throttle mode)\r\n");
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // 🌀 风扇速度控制: FAN:0-100
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "FAN:", 4) == 0)
    {
        int speed = atoi(cmd + 4);
        
        // 参数范围检查
        if(speed < 0) speed = 0;
        if(speed > 100) speed = 100;
        
        // 只在非油门模式下允许控制
        if(wuhuaqi_state != 2)
        {
            Num = speed;
            
            // PWM输出 (Num * 10 映射到 0-1000)
            uint16_t pwm_value = Num * 10;
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_1, pwm_value);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, pwm_value);
            
            printf("[BLE] FAN:%d PWM:%d\r\n", speed, pwm_value);
            
            // 如果当前在界面1，更新LCD显示
            if(ui == 1)
            {
                LCD_picture(Num, speed_value);
            }
        }
        else
        {
            printf("[BLE] FAN ignored (throttle mode)\r\n");
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // 📏 单位切换控制: UNIT:0 (km/h) 或 UNIT:1 (mph)
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "UNIT:", 5) == 0)
    {
        int unit = atoi(cmd + 5);
        if(unit == 0 || unit == 1)
        {
            speed_value = (uint8_t)unit;
            printf("[BLE] UNIT SET TO: %s\r\n", unit == 0 ? "km/h" : "mph");
            
            // 如果当前在界面1，更新LCD显示
            if(ui == 1)
            {
                lcd_wuhuaqi(wuhuaqi_state, speed_value);
                LCD_picture(Num, speed_value); // 刷新数字显示单位
            }
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // 📺 LCD控制: LCD:0 (熄屏) 或 LCD:1 (开屏)
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "LCD:", 4) == 0)
    {
        int state = atoi(cmd + 4);
        if(state == 0)
        {
            // 熄屏：清空LCD并设置ui为无效值
            LCD_Fill(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK);  // 填充黑色
            ui = 255;  // 设置为无效值，阻止LCD()函数更新
            printf("[BLE] LCD OFF\r\n");
        }
        else
        {
            // 开屏：恢复到界面1
            ui = 1;
            chu = 1;  // 触发重新绘制
            printf("[BLE] LCD ON\r\n");
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // ════════════════════════════════════════════════════════════
    // 📺 UI界面切换控制: UI:0-4
    // ════════════════════════════════════════════════════════════
    // 🔴 重要修复：chu 变量是界面初始化的"门卫"，必须设置正确的值！
    // - ui=0 时需要 chu=0，进入后置为 1
    // - ui=1 时需要 chu=1，进入后置为 2
    // - ui=2 时需要 chu=2，进入后置为 3
    // - ui=3 时需要 chu=3，进入后置为 4
    // - ui=4 时需要 chu=4，进入后置为 0
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "UI:", 3) == 0)
    {
        int target_ui = atoi(cmd + 3);
        if(target_ui >= 1 && target_ui <= 4)
        {
            // ✅ 菜单界面重构：先切换到菜单，选中目标 UI，然后自动进入
            ui = 5;
            chu = 5;
            menu_selected = (uint8_t)target_ui;
            auto_enter = 1;  // 标志位：菜单初始化后自动进入
            
            printf("[BLE] UI SWITCH TO: %d via menu\r\n", target_ui);
        }
        else if(target_ui == 0)
        {
            // UI0 开机动画界面特殊处理
            ui = (u8)target_ui;
            chu = 0;
            printf("[BLE] UI SWITCH TO: %d, chu=%d\r\n", ui, chu);
        }
        else if(target_ui == 6)
        {
            // 🆕 UI6 Logo预设界面：直接切换，不经过菜单
            ui = 6;
            chu = 6;  // 触发Logo界面初始化
            printf("[BLE] UI SWITCH TO: 6 (Logo)\r\n");
        }
        else
        {
            printf("[BLE] UI invalid: %d\r\n", target_ui);
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // 🎨 LED预设颜色方案: PRESET:1-12
    // 对应硬件 deng_ui2() 函数中的12种配色
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "PRESET:", 7) == 0)
    {
        int preset = atoi(cmd + 7);
        if(preset >= 1 && preset <= 12)
        {
            deng_num = (u8)preset; // ✅ 核心修复：更新硬件全局预设索引
            preset_dirty = 1;       // ✅ 标记：LCD 进入预设界面时强制刷新
            // 根据预设设置 LED2 和 LED3 的颜色 (与 deng_ui2() 保持一致)
            switch(preset) {
                case 1:  // 赛博霓虹 - 蓝紫→春绿
                    red2 = 138; green2 = 43;  blue2 = 226;
                    red3 = 0;   green3 = 255; blue3 = 128;
                    break;
                case 2:  // 冰晶青 - 青色纯色
                    red2 = 0;   green2 = 234; blue2 = 255;
                    red3 = 0;   green3 = 234; blue3 = 255;
                    break;
                case 3:  // 日落熔岩 - 橙红→天青
                    red2 = 255; green2 = 100; blue2 = 0;
                    red3 = 0;   green3 = 200; blue3 = 255;
                    break;
                case 4:  // 竞速黄 - 黄色纯色
                    red2 = 255; green2 = 210; blue2 = 0;
                    red3 = 255; green3 = 210; blue3 = 0;
                    break;
                case 5:  // 烈焰红 - 红色纯色
                    red2 = 255; green2 = 0;   blue2 = 0;
                    red3 = 255; green3 = 0;   blue3 = 0;
                    break;
                case 6:  // 警灯双闪 - 红→警蓝
                    red2 = 255; green2 = 0;   blue2 = 0;
                    red3 = 0;   green3 = 80;  blue3 = 255;
                    break;
                case 7:  // 樱花绯红 - 热粉→玫红
                    red2 = 255; green2 = 105; blue2 = 180;
                    red3 = 255; green3 = 0;   blue3 = 80;
                    break;
                case 8:  // 极光幻紫 - 紫→青绿
                    red2 = 180; green2 = 0;   blue2 = 255;
                    red3 = 0;   green3 = 255; blue3 = 200;
                    break;
                case 9:  // 暗夜紫晶 - 紫色纯色
                    red2 = 148; green2 = 0;   blue2 = 211;
                    red3 = 148; green3 = 0;   blue3 = 211;
                    break;
                case 10: // 薄荷清风 - 薄荷→天蓝
                    red2 = 0;   green2 = 255; blue2 = 180;
                    red3 = 100; green3 = 200; blue3 = 255;
                    break;
                case 11: // 丛林猛兽 - 荧光绿纯色
                    red2 = 0;   green2 = 255; blue2 = 65;
                    red3 = 0;   green3 = 255; blue3 = 65;
                    break;
                case 12: // 纯净白 - 暖白纯色
                    red2 = 225; green2 = 225; blue2 = 225;
                    red3 = 225; green3 = 225; blue3 = 225;
                    break;
            }
            
            // 同时设置 LED1 和 LED4 为相同颜色（统一效果）
            red1 = red2;   green1 = green2;   blue1 = blue2;
            red4 = red3;   green4 = green3;   blue4 = blue3;
            
            // 更新所有灯带
            // 先写颜色，再逐条刷新，避免只更新后两条灯带导致“无反应”
            WS2812B_SetAllLEDs(1, red1*bright*bright_num, green1*bright*bright_num, blue1*bright*bright_num);
            WS2812B_SetAllLEDs(2, red2*bright*bright_num, green2*bright*bright_num, blue2*bright*bright_num);
            WS2812B_SetAllLEDs(3, red3*bright*bright_num, green3*bright*bright_num, blue3*bright*bright_num);
            WS2812B_SetAllLEDs(4, red4*bright*bright_num, green4*bright*bright_num, blue4*bright*bright_num);
            WS2812B_Update(1);
            WS2812B_Update(2);
            WS2812B_Update(3);
            WS2812B_Update(4);

            // ✅ 如果当前在配色预设界面 (ui=2)，立即刷新 LCD 界面同步 App 的选择
            if(ui == 2)
            {
                deng_ui2();                  // 刷新预设逻辑（更新 _zhong 变量）
                lcd_pei_se(deng_num, pei_se_state); // 立即刷新 LCD 颜色条
                preset_dirty = 0;            // 已实时刷新，清除标志
            }
            
            // printf("[BLE] PRESET:%d applied\r\n", preset); // 移除高频打印
        }
        else
        {
            printf("[BLE] PRESET invalid: %d\r\n", preset);
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // 🔄 流水灯控制: STREAMLIGHT:0 (关闭) 或 STREAMLIGHT:1 (开启)
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "STREAMLIGHT:", 12) == 0)
    {
        int state = atoi(cmd + 12);
        extern u8 deng_2or3;  // 流水灯开关变量
        
        if(state == 1)
        {
            deng_2or3 = 1;  // 开启流水灯
            printf("[BLE] STREAMLIGHT ON\r\n");
        }
        else
        {
            deng_2or3 = 0;  // 关闭流水灯
            printf("[BLE] STREAMLIGHT OFF\r\n");
        }
        
        // 立即应用LED效果
        deng();
        
        // 如果当前在配色预设界面 (ui=2)，更新LCD显示
        if(ui == 2)
        {
            extern void lcd_pei_se_dot(u8 flow_state);
            lcd_pei_se_dot(deng_2or3);
        }
        
        // 发送确认响应
        char response[32];
        sprintf(response, "OK:STREAMLIGHT:%d\r\n", deng_2or3);
        HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
    }
    
    // ════════════════════════════════════════════════════════════
    // 💡 亮度调节控制: BRIGHT:0-100
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "BRIGHT:", 7) == 0)
    {
        int b = atoi(cmd + 7);
        if(b < 0) b = 0;
        if(b > 100) b = 100;
        bright = (int16_t)b;
        
        // 立即刷新所有灯带亮度
        deng();
        
        // 如果当前在界面4，更新LCD显示
        if(ui == 4)
        {
            LCD_ui4_num_update(bright);
        }
        printf("[BLE] BRIGHT SET TO: %d\r\n", bright);
    }
    
    // ✅ LED渐变控制: LED_GRADIENT:strip:r:g:b:speed
    // strip: 1=M(中), 2=L(左), 3=R(右), 4=B(后)
    // r,g,b: 0-255
    // speed: 0=快速, 1=正常, 2=慢速
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "LED_GRADIENT:", 13) == 0)
    {
        int strip, r, g, b, speed;
        
        // 解析参数: LED_GRADIENT:strip:r:g:b:speed
        char* p = cmd + 13;
        strip = atoi(p);
        
        p = strchr(p, ':');
        if(p == NULL) return;
        r = atoi(++p);
        
        p = strchr(p, ':');
        if(p == NULL) return;
        g = atoi(++p);
        
        p = strchr(p, ':');
        if(p == NULL) return;
        b = atoi(++p);
        
        p = strchr(p, ':');
        if(p == NULL) speed = 1;  // 默认正常速度
        else speed = atoi(++p);
        
        // 参数范围检查
        if(strip < 1 || strip > 4) return;
        if(r < 0) r = 0; if(r > 255) r = 255;
        if(g < 0) g = 0; if(g > 255) g = 255;
        if(b < 0) b = 0; if(b > 255) b = 255;
        
        // 选择渐变速度
        uint16_t speed_mode;
        switch(speed) {
            case 0: speed_mode = GRADIENT_SPEED_FAST; break;
            case 2: speed_mode = GRADIENT_SPEED_SLOW; break;
            default: speed_mode = GRADIENT_SPEED_NORMAL; break;
        }
        
        // 更新全局颜色变量
        switch(strip)
        {
            case 1:
                red1 = r; green1 = g; blue1 = b;
                red1_zhong = r; green1_zhong = g; blue1_zhong = b;
                break;
            case 2:
                red2 = r; green2 = g; blue2 = b;
                red2_zhong = r; green2_zhong = g; blue2_zhong = b;
                break;
            case 3:
                red3 = r; green3 = g; blue3 = b;
                red3_zhong = r; green3_zhong = g; blue3_zhong = b;
                break;
            case 4:
                red4 = r; green4 = g; blue4 = b;
                red4_zhong = r; green4_zhong = g; blue4_zhong = b;
                break;
        }
        
        // 启动渐变（LED索引从0开始）
        LED_StartGradient(strip - 1, r, g, b, speed_mode);
        
        printf("[BLE] LED_GRADIENT: strip=%d, RGB=(%d,%d,%d), speed=%d\r\n", strip, r, g, b, speed);
    }
    
    // 💡 LED颜色控制: LED:strip:r:g:b
    // strip: 1=M(中), 2=L(左), 3=R(右), 4=B(后)
    // r,g,b: 0-255
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "LED:", 4) == 0)
    {
        int strip, r, g, b;
        
        // 解析参数: LED:strip:r:g:b
        char* p = cmd + 4;
        strip = atoi(p);
        
        p = strchr(p, ':');
        if(p == NULL) return;
        r = atoi(++p);
        
        p = strchr(p, ':');
        if(p == NULL) return;
        g = atoi(++p);
        
        p = strchr(p, ':');
        if(p == NULL) return;
        b = atoi(++p);
        
        // 参数范围检查
        if(strip < 1 || strip > 4) return;
        if(r < 0) r = 0; if(r > 255) r = 255;
        if(g < 0) g = 0; if(g > 255) g = 255;
        if(b < 0) b = 0; if(b > 255) b = 255;
        
        // 更新对应灯带的颜色值
        // ✅ 核心修复：同步更新 _zhong 变量，确保 LCD 实时显示 App 调色数值
        switch(strip)
        {
            case 1:  // M - 中间
                red1 = r; green1 = g; blue1 = b;
                red1_zhong = r; green1_zhong = g; blue1_zhong = b;
                WS2812B_SetAllLEDs(1, r*bright*bright_num, g*bright*bright_num, b*bright*bright_num);
                WS2812B_Update(1);
                break;
            case 2:  // L - 左侧
                red2 = r; green2 = g; blue2 = b;
                red2_zhong = r; green2_zhong = g; blue2_zhong = b;
                WS2812B_SetAllLEDs(2, r*bright*bright_num, g*bright*bright_num, b*bright*bright_num);
                WS2812B_Update(2);
                break;
            case 3:  // R - 右侧
                red3 = r; green3 = g; blue3 = b;
                red3_zhong = r; green3_zhong = g; blue3_zhong = b;
                WS2812B_SetAllLEDs(3, r*bright*bright_num, g*bright*bright_num, b*bright*bright_num);
                WS2812B_Update(3);
                break;
            case 4:  // B - 后部
                red4 = r; green4 = g; blue4 = b;
                red4_zhong = r; green4_zhong = g; blue4_zhong = b;
                WS2812B_SetAllLEDs(4, r*bright*bright_num, g*bright*bright_num, b*bright*bright_num);
                WS2812B_Update(4);
                break;
        }

        // ✅ 在 RGB 调色界面 (ui=3)，智能判断显示模式
        if(ui == 3)
        {
            // 底部标签：0=Middle, 1=Left, 2=Right, 3=Back
            name = (u8)(strip - 1);
            lcd_name_update(name);

            // 获取该灯带之前的RGB值，判断哪个通道发生了变化
            static u8 last_r[5] = {0}, last_g[5] = {0}, last_b[5] = {0};
            static u8 active_channel = 0;  // 0=无, 4=R, 5=G, 6=B
            static u32 last_led_tick = 0;  // 上次LED命令时间
            static u8 led_cmd_count = 0;   // 连续LED命令计数
            static u8 streamlight_mode = 0; // 流水灯模式标志
            
            u8 r_changed = (r != last_r[strip]);
            u8 g_changed = (g != last_g[strip]);
            u8 b_changed = (b != last_b[strip]);
            
            // 检测流水灯模式：短时间内收到多个灯带的命令，且RGB同时变化
            u32 now = uwTick;
            if(now - last_led_tick < 100) {
                led_cmd_count++;
                if(led_cmd_count >= 3 && (r_changed && g_changed && b_changed)) {
                    streamlight_mode = 1;  // 进入流水灯模式
                }
            } else {
                led_cmd_count = 1;
                // 如果间隔较长，退出流水灯模式
                if(now - last_led_tick > 500) {
                    streamlight_mode = 0;
                }
            }
            last_led_tick = now;
            
            // 流水灯模式：只显示字母，颜色随灯光变化
            if(streamlight_mode) {
                // 保持显示R、G、B字母，不显示数字
                // 字母颜色可以根据当前灯光颜色变化（可选）
                lcd_rgb_update(4);  // R显示字母
                lcd_rgb_update(5);  // G显示字母
                lcd_rgb_update(6);  // B显示字母
                active_channel = 0;
            }
            // 普通调色模式：只更新发生变化的通道
            else if(r_changed && !g_changed && !b_changed) {
                // 只有R变化
                if(active_channel != 4) {
                    lcd_rgb_update(5);  // G显示字母
                    lcd_rgb_update(6);  // B显示字母
                }
                lcd_rgb_num(4, r);
                active_channel = 4;
            }
            else if(!r_changed && g_changed && !b_changed) {
                // 只有G变化
                if(active_channel != 5) {
                    lcd_rgb_update(4);  // R显示字母
                    lcd_rgb_update(6);  // B显示字母
                }
                lcd_rgb_num(5, g);
                active_channel = 5;
            }
            else if(!r_changed && !g_changed && b_changed) {
                // 只有B变化
                if(active_channel != 6) {
                    lcd_rgb_update(4);  // R显示字母
                    lcd_rgb_update(5);  // G显示字母
                }
                lcd_rgb_num(6, b);
                active_channel = 6;
            }
            else if(r_changed || g_changed || b_changed) {
                // 多个通道同时变化但不是流水灯模式（如首次设置）
                // 也显示字母，不显示数字
                lcd_rgb_update(4);
                lcd_rgb_update(5);
                lcd_rgb_update(6);
                active_channel = 0;
            }
            
            // 更新记录
            last_r[strip] = r;
            last_g[strip] = g;
            last_b[strip] = b;
        }
        
        printf("[BLE] LED:%d R:%d G:%d B:%d\r\n", strip, r, g, b);
    }
    
    // ════════════════════════════════════════════════════════════
    // 🏎️ 运行模式速度同步: SPEED:0-340
    // 同时更新 LCD 显示和风扇速度
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "SPEED:", 6) == 0)
    {
        int speed = atoi(cmd + 6);
        if(speed < 0) speed = 0;
        if(speed > 340) speed = 340;
        
        // ✅ 精度修复：直接存储绝对速度，避免 Num 映射精度损失
        current_speed_kmh = (int16_t)speed;
        
        // 🔒 防止反馈循环：更新 last_reported_speed，避免 Encoder() 重复上报
        extern int16_t last_reported_speed;
        last_reported_speed = (int16_t)speed;
        
        // ✅ 同步更新 Num 变量，确保硬件端旋钮操作时能接续当前速度
        // 将 speed (0-340) 映射回 Num (0-100)
        // ✅ 精度修复：使用四舍五入而非截断，减少精度损失
        Num = (speed * 100 + 170) / 340;  // +170 实现四舍五入 (340/2=170)
        if (Num > 100) Num = 100;
        if (Num < 0) Num = 0;
        
        // ════════════════════════════════════════════════════════════
        // 🔇 软件消噪方案 v3：二值输出（只有 0% 和 100%）
        // 原理：任何中间占空比都会产生 500Hz 开关噪音
        //       只用全开/全关，彻底消除 PWM 噪音
        // ════════════════════════════════════════════════════════════
        const int fan_threshold = 60;  // 速度阈值（可调：降低则更早开风扇）
        uint16_t pwm_value;
        
        if(speed <= fan_threshold) {
            // 低于阈值：完全停转（静音）
            pwm_value = 0;
        } else {
            // 高于阈值：满档（无 PWM 开关，无噪音）
            pwm_value = 1000;
        }
        
        __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_1, pwm_value);
        __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, pwm_value);
        
        // 3. 更新音频和界面状态
        if(wuhuaqi_state == 2)
        {
            uint8_t target_volume = (Num > 0) ? (30 + (Num * 70 / 100)) : 0;
            EngineAudio_SetVolume(target_volume);
            
            // ✅ 终极接管：记录活跃时间，彻底瘫痪硬件本地自动减速逻辑
            wuhuaqi_state = 2;
            wuhuaqi_state_old = 2; 
            extern uint32_t remote_active_tick;
            remote_active_tick = uwTick;
        }

        // 4. 强制刷新屏幕数字和单位
        if(ui == 1)
        {
            // ✅ 修复：每次刷新数字后都刷新单位，确保单位始终可见
            LCD_picture(speed, 2);
            lcd_wuhuaqi(wuhuaqi_state, speed_value);  // 每次都刷新单位
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // 🔥 远程油门模式控制: THROTTLE:1 (开启) 或 THROTTLE:0 (关闭)
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "THROTTLE:", 9) == 0)
    {
        int state = atoi(cmd + 9);
        if(state == 1)
        {
            // 远程模拟三击：进入油门模式
            wuhuaqi_state_saved = wuhuaqi_state;
            wuhuaqi_state = 2;
            wuhuaqi_state_old = 2;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
            lcd_wuhuaqi(wuhuaqi_state, speed_value);
            EngineAudio_Start(); // 启动硬件本地音效
            printf("[BLE] REMOTE THROTTLE START\r\n");
        }
        else
        {
            // 远程退出油门模式
            wuhuaqi_state = wuhuaqi_state_saved;
            wuhuaqi_state_old = wuhuaqi_state;
            
            // ✅ 修复：退出油门模式时强制将 Num 设为 0
            Num = 0;
            
            // 同步更新 PWM
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_1, 0);
            __HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, 0);
            
            if (wuhuaqi_state == 0) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
            
            // 刷新 LCD 显示
            if(ui == 1) {
                LCD_picture(0, 2);  // 显示 0
            }
            lcd_wuhuaqi(wuhuaqi_state, speed_value);
            EngineAudio_Stop(); // 停止硬件本地音效
            printf("[BLE] REMOTE THROTTLE STOP, Num reset to 0\r\n");
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // 📡 查询命令: GET:xxx
    // 支持: GET:FAN, GET:WUHUA, GET:BRIGHT, GET:ALL
    // ════════════════════════════════════════════════════════════
    else if(strncmp(cmd, "GET:", 4) == 0)
    {
        char* param = cmd + 4;
        char response[64];
        
        if(strcmp(param, "FAN") == 0)
        {
            // 返回风扇速度
            sprintf(response, "FAN:%d\r\n", Num);
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            printf("[BLE] GET:FAN -> %d\r\n", Num);
        }
        else if(strcmp(param, "WUHUA") == 0)
        {
            // 返回雾化器状态
            sprintf(response, "WUHUA:%d\r\n", wuhuaqi_state == 1 ? 1 : 0);
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            printf("[BLE] GET:WUHUA -> %d\r\n", wuhuaqi_state);
        }
        else if(strcmp(param, "BRIGHT") == 0)
        {
            // 返回亮度
            sprintf(response, "BRIGHT:%d\r\n", bright);
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            printf("[BLE] GET:BRIGHT -> %d\r\n", bright);
        }
        else if(strcmp(param, "STREAMLIGHT") == 0)
        {
            // 🔄 返回流水灯状态
            extern u8 deng_2or3;
            sprintf(response, "STREAMLIGHT:%d\r\n", deng_2or3);
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            printf("[BLE] GET:STREAMLIGHT -> %d\r\n", deng_2or3);
        }
        else if(strcmp(param, "PRESET") == 0)
        {
            // 🎨 返回当前LED预设索引
            sprintf(response, "PRESET_REPORT:%d\r\n", deng_num);
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            printf("[BLE] GET:PRESET -> %d\r\n", deng_num);
        }
        else if(strcmp(param, "ALL") == 0)
        {
            // 返回所有状态
            sprintf(response, "STATUS:FAN:%d:WUHUA:%d:BRIGHT:%d\r\n", 
                    Num, 
                    wuhuaqi_state == 1 ? 1 : 0, 
                    bright);
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            printf("[BLE] GET:ALL\r\n");
        }
        else if(strcmp(param, "UI") == 0)
        {
            // 返回当前界面
            sprintf(response, "UI:%d\r\n", ui);
            HAL_UART_Transmit(&huart2, (uint8_t*)response, strlen(response), 100);
            printf("[BLE] GET:UI -> %d\r\n", ui);
        }
        else if(strcmp(param, "LOGO") == 0)
        {
            // 🆕 查询是否有自定义Logo - 转发到Logo模块处理
            Logo_ParseCommand(cmd);
        }
        else
        {
            printf("[BLE] GET unknown: %s\r\n", param);
        }
    }
    
    // ════════════════════════════════════════════════════════════
    // 🆕 Logo上传相关命令: LOGO_START, LOGO_DATA, LOGO_END, LOGO_DELETE, LOGO_STATUS
    // ════════════════════════════════════════════════════════════
    else if (strncmp(cmd, "LOGO_", 5) == 0 || strcmp(cmd, "GET:LOGO") == 0)
    {
        // 🔧 优化：只在START命令时发送调试信息，DATA命令不发送（减少蓝牙负担）
        if (strncmp(cmd, "LOGO_START", 10) == 0) {
            char debug_msg[64];
            snprintf(debug_msg, sizeof(debug_msg), "DEBUG:rx_START_ok\n");
            BLE_SendString(debug_msg);
        }
        
        Logo_ParseCommand(cmd);
    }
    
    // ════════════════════════════════════════════════════════════
    // 未知命令
    // ════════════════════════════════════════════════════════════
    else
    {
        printf("[BLE] Unknown: %s\r\n", cmd);
    }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          接收处理函数 (支持一包多命解析)                      ║
// ║          ✅ 优化：检测到换行符立即解析，减少延迟              ║
// ╚══════════════════════════════════════════════════════════════╝
void RX_proc(void)
{
    // ✅ 优化：有数据时检查是否有完整指令（以换行符结尾）
    if(rx_pointer > 0)
    {
        // 检查缓冲区末尾是否有换行符，或者超时
        uint8_t has_complete_cmd = 0;
        uint8_t has_newline = 0;
        
        // 检查是否有换行符
        if(rx_buff[rx_pointer-1] == '\n' || rx_buff[rx_pointer-1] == '\r') {
            has_newline = 1;
            has_complete_cmd = 1;  // 收到完整指令，立即解析
        }
        
        // 🔥 特殊处理：如果是Logo命令，必须等到换行符才解析（不使用超时）
        // 因为Logo命令很长（50+字节），蓝牙会分包发送，不能提前解析
        if(!has_newline && rx_pointer >= 5 && strncmp((char*)rx_buff, "LOGO_", 5) == 0) {
            // 这是Logo命令，但还没收到换行符，继续等待
            // 只有超过500ms才强制解析（避免死锁）
            if(uwTick - rx_tick >= 500) {
                has_complete_cmd = 1;
                char debug_msg[64];
                snprintf(debug_msg, sizeof(debug_msg), "DEBUG:Logo_timeout_len=%d\n", rx_pointer);
                BLE_SendString(debug_msg);
            } else {
                return;  // 继续等待
            }
        }
        // 普通命令：30ms超时
        else if(!has_newline && (uwTick - rx_tick >= 30)) {
            has_complete_cmd = 1;  // 超时30ms，强制解析（处理不带换行的异常数据）
        }
        
        if(!has_complete_cmd) {
            return;  // 继续等待更多数据
        }
        
        // 🔧 修复竞争条件：先禁用中断，复制数据，再清空缓冲区
        __disable_irq();
        
        // 添加字符串结束符
        rx_buff[rx_pointer] = '\0';
        uint16_t saved_pointer = rx_pointer;
        
        // 复制到临时缓冲区
        static char temp_buff[RX_BUFFER_SIZE];
        memcpy(temp_buff, rx_buff, saved_pointer + 1);
        
        // 清空缓冲区
        rx_pointer = 0;
        memset(rx_buff, 0, saved_pointer + 1);
        
        __enable_irq();
        
        // ✅ 循环解析临时缓冲区中的所有指令
        // 使用 strtok 按 \n 或 \r 分割，支持一包数据中包含多条命令
        char* token = strtok(temp_buff, "\n\r");
        while(token != NULL)
        {
            if(strlen(token) > 0) {
                BLE_ParseCommand(token);
            }
            token = strtok(NULL, "\n\r");
        }
    }
}

// ╔══════════════════════════════════════════════════════════════╗
// ║          UART接收中断回调                                     ║
// ╚══════════════════════════════════════════════════════════════╝
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart == &huart2)
    {
        rx_tick = uwTick;
        
        // 继续接收下一个字节
        HAL_UART_Receive_IT(&huart2, &rx_data, 1);
        
        // 存入缓冲区（防止溢出）
        if(rx_pointer < RX_BUFFER_SIZE - 1)
        {
            rx_buff[rx_pointer++] = rx_data;
        }
    }
}



// ╔══════════════════════════════════════════════════════════════╗
// ║          🆕 蓝牙主动上报函数                                  ║
// ║          用于硬件主动向APP发送数据                            ║
// ╚══════════════════════════════════════════════════════════════╝

/**
 * @brief  通过蓝牙发送字符串
 * @param  str: 要发送的字符串
 */
void BLE_SendString(const char* str)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)str, strlen(str), 100);
}

/**
 * @brief  上报旋钮增量到APP
 * @param  delta: 增量值（正=顺时针，负=逆时针）
 * @note   协议格式: KNOB:delta\n
 */
void BLE_ReportKnobDelta(int16_t delta)
{
    if(delta == 0) return;  // 无变化不上报
    
    char buf[16];
    sprintf(buf, "KNOB:%d\n", delta);
    BLE_SendString(buf);
}

/**
 * @brief  上报按钮事件到APP
 * @param  type: 按钮类型 (KNOB, POWER等)
 * @param  action: 动作类型 (CLICK, LONG, DOUBLE, TRIPLE)
 * @note   协议格式: BTN:type:action\n
 */
void BLE_ReportButtonEvent(const char* type, const char* action)
{
    char buf[32];
    sprintf(buf, "BTN:%s:%s\n", type, action);
    BLE_SendString(buf);
}

/**
 * @brief  上报绝对速度到APP
 * @param  speed: 当前速度值 (0-340)
 * @param  unit: 单位 (0=km/h, 1=mph)
 * @note   协议格式: SPEED_REPORT:value:unit\n
 */
void BLE_ReportSpeed(int16_t speed, uint8_t unit)
{
    // 范围检查
    if (speed < 0) speed = 0;
    if (speed > 340) speed = 340;
    if (unit > 1) unit = 0;
    
    char buf[24];
    sprintf(buf, "SPEED_REPORT:%d:%d\n", speed, unit);
    BLE_SendString(buf);
}

/**
 * @brief  上报油门模式状态到APP
 * @param  state: 0=退出油门模式, 1=进入油门模式
 * @note   协议格式: THROTTLE_REPORT:state\n
 */
void BLE_ReportThrottle(uint8_t state)
{
    char buf[20];
    sprintf(buf, "THROTTLE_REPORT:%d\n", state ? 1 : 0);
    BLE_SendString(buf);
}

/**
 * @brief  上报单位切换到APP
 * @param  unit: 0=km/h, 1=mph
 * @note   协议格式: UNIT_REPORT:unit\n
 */
void BLE_ReportUnit(uint8_t unit)
{
    char buf[16];
    sprintf(buf, "UNIT_REPORT:%d\n", unit ? 1 : 0);
    BLE_SendString(buf);
}

/**
 * @brief  上报颜色预设切换到APP
 * @param  preset: 预设索引 (1-12)
 * @note   协议格式: PRESET_REPORT:preset\n
 */
void BLE_ReportPreset(uint8_t preset)
{
    if (preset < 1) preset = 1;
    if (preset > 12) preset = 12;
    
    char buf[20];
    sprintf(buf, "PRESET_REPORT:%d\n", preset);
    BLE_SendString(buf);
}
