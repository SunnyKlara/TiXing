#include "xuanniu.h"
#include "engine_audio.h"
#include "rx.h"  // 🆕 蓝牙上报函数
#include "logo.h"  // Logo显示函数
#include <math.h>  // sinf函数
uint8_t KeyNum = 0,speed_value = 0,speed_value_old = 0;

// ✅ 菜单界面重构：新增全局变量定义
uint8_t menu_selected = 1;      // 菜单选中的 UI（1-6），默认选中 UI1
int16_t encoder_delta = 0;      // 编码器增量（每帧读取一次）
uint8_t auto_enter = 0;         // 蓝牙命令自动进入标志
int16_t Num,Num_old,led_num_old,led_num = 3,bright = 100,bright_old;			//定义待被旋转编码器调节的变量
uint8_t led_stick  = 1,chu = 0,num_stick = 0,lian_num = 10,ui2_num = 0,ui1_num = 0,ui3_num = 0,tiao_se_num = 0,tiao_se_num_old = 0,led_clear = 0;

// ═══════════════════════════════════════════════════════════════════
// 🎨 UI3 RGB调色界面状态机 - 重构优化
// ═══════════════════════════════════════════════════════════════════
// ui3_mode: 0=选择灯带模式, 1=调节RGB模式
// ui3_channel: 当前调节的通道 (0=R, 1=G, 2=B)
// name: 当前选中的灯带 (0=Middle, 1=Left, 2=Right, 3=Back)
uint8_t ui3_mode = 0;       // UI3模式：0=选灯带, 1=调RGB
uint8_t ui3_channel = 0;    // RGB通道：0=R, 1=G, 2=B
uint8_t red4=255,red2=255,red3=33,red1=150,green4=0,green2=0,green3=126,green1=20,blue4=0,blue2=0,blue3=222,blue1=0;
uint8_t red4_old=0,red2_old=0,red3_old=0,red1_old=0,green4_old=0,green2_old=0,green3_old=0,green1_old=0,blue4_old=0,blue2_old=0,blue3_old=0,blue1_old=0;
int16_t red4_zhong=255,red2_zhong=255,red3_zhong=33,red1_zhong=150,green4_zhong=0,green2_zhong=0,green3_zhong=126,green1_zhong=20,blue4_zhong=0,blue2_zhong=0,blue3_zhong=222,blue1_zhong=0;
int16_t red4_zhong_old=0,red2_zhong_old=0,red3_zhong_old=0,red1_zhong_old=150,green4_zhong_old=0,green2_zhong_old=0,green3_zhong_old=0,green1_zhong_old=20,blue4_zhong_old=0,blue2_zhong_old=0,blue3_zhong_old=0,blue1_zhong_old=0;

// ✅ 精度修复：引用绝对速度变量（定义在 rx.c）
extern int16_t current_speed_kmh;
int8_t name = 0,name_old=9;
u8 red_width=0,green_width=0,blue_width=0,red_width_old=0,green_width_old=0,blue_width_old=0;
u8 red_num,green_num,blue_num;
//uint8_t ArrayRead[8192];								//定义要读取数据的测试数组
uint8_t WriteRead[16];  // ✅ 扩展为16字节，新增音量存储
u8 deng_2or3 = 0;//1为2，0为3
u8 volume = 80, volume_old = 80;  // ✅ 音量变量 (0-100)，默认80%
int8_t deng_num = 1;
u8 deng_num_old,bright_wid,bright_wid_old;
u8 wuhuaqi_state = 1,wuhuaqi_state_old = 1,pei_se_state = 0,pei_se_state_old = 0,brt_state = 0,brt_nothing = 0;  // 🆕 默认开启雾化器
u8 wuhuaqi_state_saved = 1;  // 专门用于保存进入油门模式前的状态（用于退出时恢复）// 🆕 默认开启
volatile uint8_t preset_dirty = 0; // ✅ 记录预设被远程修改，进入配色界面时强制刷新LCD

// ═══════════════════════════════════════════════════════════════════
// 🌟 UI4 呼吸灯功能
// ═══════════════════════════════════════════════════════════════════
uint8_t breath_mode = 0;        // 呼吸灯模式：0=关闭(红点), 1=开启(绿点)
uint16_t breath_phase = 0;      // 呼吸灯相位 (0-628, 对应0-2π)
uint32_t breath_tick = 0;       // 呼吸灯刷新时间戳

// ============ LED平滑渐变过渡系统 ============
// 渐变状态结构体
typedef struct {
    uint8_t current_r, current_g, current_b;  // 当前颜色
    uint8_t target_r, target_g, target_b;     // 目标颜色
    uint8_t start_r, start_g, start_b;        // 起始颜色
    uint16_t total_frames;                     // 总帧数  
    uint16_t current_frame;                    // 当前帧
    uint8_t active;                            // 是否正在渐变
} LED_Gradient_t;

// 四组LED的渐变状态
LED_Gradient_t led_gradient[4] = {0};  // LED1-4

// 渐变速度模式
#define GRADIENT_SPEED_FAST   25   // 快速：0.5秒 (25帧 @ 50fps)
#define GRADIENT_SPEED_NORMAL 75   // 正常：1.5秒 (75帧 @ 50fps)
#define GRADIENT_SPEED_SLOW   150  // 慢速：3秒 (150帧 @ 50fps)

// 渐变刷新时间戳
static uint32_t gradient_tick = 0;

// ✅ UI5蓝牙调试界面已删除，相关变量已移除

// 引脚定义（根据实际硬件修改，确保与CubeMX配置一致）
#define ENC_A_PIN         GPIO_PIN_8    // 编码器A相引脚
#define ENC_B_PIN         GPIO_PIN_9    // 编码器B相引脚
#define ENC_KEY_PIN       GPIO_PIN_10   // 编码器按键引脚
#define ENC_PORT          GPIOA         // 编码器引脚所在端口

// 全局变量定义
u8 key_old = 1, key_value = 1;  // 按键状态（默认高电平，未按下）
u8 key_down = 0, key_up = 0;    // 按键按下/释放标志
u8 key_state = 0;
u8 ui = 1, ui_old = 1;          // 界面切换变量（开机直接进入UI1，不再显示UI0）
u32 key_tick = 0,key_state_tick = 0;               // 按键消抖计时
u32 encoder_tick = 0;
// 编码器核心变量
volatile int16_t encoder_count = 0;    // 编码器计数值（已弃用）
static uint16_t last_encoder_cnt = 0;  // 上次读取的编码器计数器值
static uint8_t encoder_initialized = 0; // 编码器是否已初始化

#define ENCODER_CENTER_VALUE  32768    // 计数器中间值
#define ENCODER_MAX_VALUE     65535    // 计数器最大值

/**
 * @brief  初始化编码器（运行时重新配置TIM1）
 * @note   将 Period 改为 65535，CNT 初始化为中间值
 */
void Encoder_Init(void) {
    if (encoder_initialized) return;
    
    // 停止编码器
    HAL_TIM_Encoder_Stop(&htim1, TIM_CHANNEL_ALL);

    // ✅ 统一通道预分频 & 增强输入滤波，避免方向抖动/反转
    // - IC1Prescaler/IC2Prescaler 都设为 DIV1（0）
    // - 滤波设为 0b1111：fSAMPLING=fCK/32，N=8 次，最大滤波，进一步抑制毛刺
    TIM1->CCMR1 = (TIM1->CCMR1 & ~(TIM_CCMR1_IC1PSC | TIM_CCMR1_IC2PSC | TIM_CCMR1_IC1F | TIM_CCMR1_IC2F))
                  | (0 << TIM_CCMR1_IC1PSC_Pos)
                  | (0 << TIM_CCMR1_IC2PSC_Pos)
                  | (15 << TIM_CCMR1_IC1F_Pos)
                  | (15 << TIM_CCMR1_IC2F_Pos);
    
    // 修改 ARR (Auto-Reload Register) 为最大值
    __HAL_TIM_SET_AUTORELOAD(&htim1, ENCODER_MAX_VALUE);
    
    // 设置计数器为中间值，这样可以检测正负方向
    __HAL_TIM_SET_COUNTER(&htim1, ENCODER_CENTER_VALUE);
    last_encoder_cnt = ENCODER_CENTER_VALUE;
    
    // 重新启动编码器（不使用中断）
    HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
    
    encoder_initialized = 1;
}

/**
 * @brief  获取编码器旋转增量
 * @return 增量值（负值：逆时针，正值：顺时针，0：无旋转）
 * @note   ✅ 重构：直接读取TIM1计数器值，通过差值判断方向和步数
 */
int16_t Encoder_GetDelta(void) {
    // 确保编码器已初始化
    if (!encoder_initialized) {
        Encoder_Init();
    }
    
    // 读取当前计数器值
    uint16_t current_cnt = __HAL_TIM_GET_COUNTER(&htim1);
    
    // 计算增量（有符号差值）
    int16_t delta = (int16_t)(current_cnt - last_encoder_cnt);
    
    // 更新上次读取值
    last_encoder_cnt = current_cnt;
    
    // 防止计数器溢出导致的大跳变
    // 正常旋转速度下，20ms 内的增量不会超过 ±50
    if (delta > 100) {
        delta = 0;  // 可能是噪声或溢出，忽略
    } else if (delta < -100) {
        delta = 0;
    }
    
    // 四倍频模式下每格产生 4 个脉冲，除以 4 得到实际格数
    // 但为了灵敏度，我们直接使用原始值再限幅
    if (delta > 3) delta = 3;
    if (delta < -3) delta = -3;
    
    // ✅ 如果方向与预期相反，在这里取反
    // delta = -delta;  // 取消注释此行如果方向相反
    
    return delta;
}

void deng()
{
	if(deng_2or3 == 0)
	{
		// 普通模式：所有LED显示对应颜色
		WS2812B_SetAllLEDs(1,red1*bright*bright_num,green1*bright*bright_num,blue1*bright*bright_num);
		WS2812B_SetAllLEDs(2,red2*bright*bright_num,green2*bright*bright_num,blue2*bright*bright_num);
		WS2812B_SetAllLEDs(3,red3*bright*bright_num,green3*bright*bright_num,blue3*bright*bright_num);
		WS2812B_Update(3);
		WS2812B_SetAllLEDs(4,red4*bright*bright_num,green4*bright*bright_num,blue4*bright*bright_num);
		WS2812B_Update(4);
	}
	else if(deng_2or3 == 1)
	{
		// 流水灯模式：LED2和LED3使用特殊模式
		WS2812B_SetAllLEDs(1,red1*bright*bright_num,green1*bright*bright_num,blue1*bright*bright_num);  // ✅ 添加LED1
		WS2812B_Set23LEDs(2,red2*bright*bright_num,green2*bright*bright_num,blue2*bright*bright_num);
		WS2812B_Set23LEDs(3,red3*bright*bright_num,green3*bright*bright_num,blue3*bright*bright_num);
		WS2812B_Update(3);
		WS2812B_SetAllLEDs(4,red4*bright*bright_num,green4*bright*bright_num,blue4*bright*bright_num);
		WS2812B_Update(4);
	}
}

// ============ LED平滑渐变函数 ============

/**
 * @brief  启动LED渐变过渡
 * @param  led_index: LED索引 (0-3 对应 LED1-4)
 * @param  target_r, target_g, target_b: 目标颜色
 * @param  speed_mode: 速度模式 (GRADIENT_SPEED_FAST/NORMAL/SLOW)
 */
void LED_StartGradient(uint8_t led_index, uint8_t target_r, uint8_t target_g, uint8_t target_b, uint16_t speed_mode)
{
    if (led_index > 3) return;
    
    LED_Gradient_t *g = &led_gradient[led_index];
    
    // 保存当前颜色作为起点
    g->start_r = g->current_r;
    g->start_g = g->current_g;
    g->start_b = g->current_b;
    
    // 设置目标颜色
    g->target_r = target_r;
    g->target_g = target_g;
    g->target_b = target_b;
    
    // 设置帧数
    g->total_frames = speed_mode;
    g->current_frame = 0;
    
    // 激活渐变
    g->active = 1;
}

/**
 * @brief  处理LED渐变（在主循环中调用，50fps）
 * @note   使用线性插值计算当前帧颜色
 */
void LED_GradientProcess(void)
{
    // 流水灯模式下不执行渐变处理，避免冲突闪烁
    if (deng_2or3 == 1) return;
    
    // 50fps刷新率控制（每20ms执行一次）
    if (uwTick - gradient_tick < 20) return;
    gradient_tick = uwTick;
    
    uint8_t any_active = 0;
    
    for (uint8_t i = 0; i < 4; i++) {
        LED_Gradient_t *g = &led_gradient[i];
        
        if (!g->active) continue;
        any_active = 1;
        
        g->current_frame++;
        
        if (g->current_frame >= g->total_frames) {
            // 渐变完成，设置为目标颜色
            g->current_r = g->target_r;
            g->current_g = g->target_g;
            g->current_b = g->target_b;
            g->active = 0;
        } else {
            // 线性插值计算当前颜色
            float progress = (float)g->current_frame / g->total_frames;
            g->current_r = g->start_r + (int16_t)(g->target_r - g->start_r) * progress;
            g->current_g = g->start_g + (int16_t)(g->target_g - g->start_g) * progress;
            g->current_b = g->start_b + (int16_t)(g->target_b - g->start_b) * progress;
        }
        
        // 更新LED颜色（考虑亮度）
        uint8_t r = (uint8_t)(g->current_r * bright * bright_num);
        uint8_t g_color = (uint8_t)(g->current_g * bright * bright_num);
        uint8_t b = (uint8_t)(g->current_b * bright * bright_num);
        
        WS2812B_SetAllLEDs(i + 1, r, g_color, b);
    }
    
    // 如果有任何LED在渐变，更新显示
    if (any_active) {
        WS2812B_Update(3);
        WS2812B_Update(4);
    }
}

// ============ 流水灯效果处理 ============
// 流水灯状态变量
static uint32_t streamlight_tick = 0;      // 流水灯刷新时间戳
static uint8_t streamlight_position = 0;   // 当前亮灯位置 (0-5: 6个LED位置)
static uint8_t streamlight_color_index = 0; // 当前颜色索引
static uint16_t streamlight_phase = 0;     // 呼吸灯相位 (0-359度)

// 流水灯颜色预设（14种配色）
static const uint8_t streamlight_colors[14][6] = {
    // {R1, G1, B1, R2, G2, B2} - 左侧颜色, 右侧颜色
    {138, 43, 226, 0, 255, 128},    // 1. 赛博霓虹 - 蓝紫→春绿
    {0, 234, 255, 0, 234, 255},     // 2. 冰晶青 - 青色纯色
    {255, 100, 0, 0, 200, 255},     // 3. 日落熔岩 - 橙红→天青
    {255, 210, 0, 255, 210, 0},     // 4. 竞速黄 - 黄色纯色
    {255, 0, 0, 255, 0, 0},         // 5. 烈焰红 - 红色纯色
    {255, 0, 0, 0, 80, 255},        // 6. 警灯双闪 - 红→警蓝
    {255, 105, 180, 255, 0, 80},    // 7. 樱花绯红 - 热粉→玫红
    {180, 0, 255, 0, 255, 200},     // 8. 极光幻紫 - 紫→青绿
    {148, 0, 211, 148, 0, 211},     // 9. 暗夜紫晶 - 紫色纯色
    {0, 255, 180, 100, 200, 255},   // 10. 薄荷清风 - 薄荷→天蓝
    {0, 255, 65, 0, 255, 65},       // 11. 丛林猛兽 - 荧光绿纯色
    {225, 225, 225, 225, 225, 225}, // 12. 纯净白 - 暖白纯色
    {255, 80, 0, 255, 200, 50},     // 13. 烈焰橙 - 深橙→浅橙（火焰感）
    {0, 255, 255, 255, 0, 255}      // 14. 霓虹派对 - 青→品红（赛博朋克）
};

/**
 * @brief  流水灯效果处理（在主循环中调用）
 * @note   当 deng_2or3 == 1 时，实现呼吸灯式的颜色渐变流水效果
 *         在12个颜色预设之间平滑渐变切换
 *         LCD颜色条实时同步更新
 */
void Streamlight_Process(void)
{
    // 只在流水灯模式下执行
    if (deng_2or3 != 1) {
        streamlight_position = 0;
        streamlight_phase = 0;
        return;
    }
    
    // 每30ms更新一次（约33fps，保证平滑且不占用太多CPU）
    if (uwTick - streamlight_tick < 30) return;
    streamlight_tick = uwTick;
    
    // 当前颜色索引和下一个颜色索引
    uint8_t curr_idx = streamlight_color_index;
    uint8_t next_idx = (curr_idx + 1) % 14;  // 14种颜色
    
    // 计算渐变进度 (0.0 ~ 1.0)
    float progress = streamlight_phase / 100.0f;
    
    // 当前预设的颜色
    uint8_t r1_curr = streamlight_colors[curr_idx][0];
    uint8_t g1_curr = streamlight_colors[curr_idx][1];
    uint8_t b1_curr = streamlight_colors[curr_idx][2];
    uint8_t r2_curr = streamlight_colors[curr_idx][3];
    uint8_t g2_curr = streamlight_colors[curr_idx][4];
    uint8_t b2_curr = streamlight_colors[curr_idx][5];
    
    // 下一个预设的颜色
    uint8_t r1_next = streamlight_colors[next_idx][0];
    uint8_t g1_next = streamlight_colors[next_idx][1];
    uint8_t b1_next = streamlight_colors[next_idx][2];
    uint8_t r2_next = streamlight_colors[next_idx][3];
    uint8_t g2_next = streamlight_colors[next_idx][4];
    uint8_t b2_next = streamlight_colors[next_idx][5];
    
    // 插值计算当前颜色
    uint8_t r1 = (uint8_t)(r1_curr + (r1_next - r1_curr) * progress);
    uint8_t g1 = (uint8_t)(g1_curr + (g1_next - g1_curr) * progress);
    uint8_t b1 = (uint8_t)(b1_curr + (b1_next - b1_curr) * progress);
    uint8_t r2 = (uint8_t)(r2_curr + (r2_next - r2_curr) * progress);
    uint8_t g2 = (uint8_t)(g2_curr + (g2_next - g2_curr) * progress);
    uint8_t b2 = (uint8_t)(b2_curr + (b2_next - b2_curr) * progress);
    
    // 亮度系数
    float br = bright * bright_num;
    
    // 更新三个灯带（左中右，不包括尾灯）
    // LED1(中)和LED2(左)用颜色1，LED3(右)用颜色2
    WS2812B_SetAllLEDs(1, (uint8_t)(r1*br), (uint8_t)(g1*br), (uint8_t)(b1*br));  // 中
    WS2812B_SetAllLEDs(2, (uint8_t)(r1*br), (uint8_t)(g1*br), (uint8_t)(b1*br));  // 左
    WS2812B_SetAllLEDs(3, (uint8_t)(r2*br), (uint8_t)(g2*br), (uint8_t)(b2*br));  // 右
    WS2812B_Update(3);
    
    // 更新全局颜色变量（用于LCD显示）
    red2_zhong = r1;
    green2_zhong = g1;
    blue2_zhong = b1;
    red3_zhong = r2;
    green3_zhong = g2;
    blue3_zhong = b2;
    
    // 如果在ui==2界面，实时刷新LCD颜色条
    if (ui == 2) {
        // 每150ms刷新一次LCD（降低频率，减少视觉干扰）
        static uint32_t lcd_refresh_tick = 0;
        if (uwTick - lcd_refresh_tick >= 150) {
            lcd_pei_se_realtime(r1, g1, b1, r2, g2, b2);
            lcd_refresh_tick = uwTick;
        }
    }
    
    // 相位递增（每次+1，0-99循环，100次完成一个颜色过渡）
    streamlight_phase++;
    if (streamlight_phase >= 100) {
        streamlight_phase = 0;
        // 切换到下一个颜色预设
        streamlight_color_index = next_idx;
        // 同步更新deng_num（用于LCD显示当前预设编号）
        deng_num = streamlight_color_index + 1;
    }
}

/**
 * @brief  呼吸灯效果处理（在主循环中调用）
 * @note   当 breath_mode == 1 且 ui != 4 时，在UI4界面外持续运行呼吸灯效果
 *         UI4界面内的呼吸灯由 deng_ui4() 处理
 */
void Breath_Process(void)
{
    // 只在呼吸灯模式开启且不在UI4界面时执行
    // UI4界面内的呼吸灯效果由 deng_ui4() 函数处理
    if (breath_mode != 1 || ui == 4) {
        return;
    }
    
    // 每20ms更新一次呼吸效果 (50fps)
    if (uwTick - breath_tick < 20) return;
    breath_tick = uwTick;
    
    // 相位递增 (0-628 对应 0-2π)
    breath_phase += 4;  // 约3秒一个周期
    if (breath_phase >= 628) breath_phase = 0;
    
    // 使用正弦函数计算亮度 (0.2 ~ 1.0)
    float sin_val = sinf(breath_phase * 0.01f);
    float breath_factor = 0.6f + 0.4f * sin_val;
    
    // 计算当前呼吸亮度
    uint8_t breath_bright = (uint8_t)(bright * breath_factor);
    float br = breath_bright * bright_num;
    
    // 更新LED（使用当前选中的颜色预设）
    WS2812B_SetAllLEDs(1, red1 * br, green1 * br, blue1 * br);
    WS2812B_SetAllLEDs(2, red2_zhong * br, green2_zhong * br, blue2_zhong * br);
    WS2812B_SetAllLEDs(3, red3_zhong * br, green3_zhong * br, blue3_zhong * br);
    WS2812B_Update(3);
    WS2812B_SetAllLEDs(4, red4 * br, green4 * br, blue4 * br);
    WS2812B_Update(4);
}

/**
 * @brief  检查是否有LED正在渐变
 * @return 1=有渐变进行中，0=无
 */
uint8_t LED_IsGradientActive(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        if (led_gradient[i].active) return 1;
    }
    return 0;
}

/**
 * @brief  初始化LED渐变状态（从当前颜色开始）
 */
void LED_GradientInit(void)
{
    // 从全局颜色变量初始化当前颜色
    led_gradient[0].current_r = red1;
    led_gradient[0].current_g = green1;
    led_gradient[0].current_b = blue1;
    
    led_gradient[1].current_r = red2;
    led_gradient[1].current_g = green2;
    led_gradient[1].current_b = blue2;
    
    led_gradient[2].current_r = red3;
    led_gradient[2].current_g = green3;
    led_gradient[2].current_b = blue3;
    
    led_gradient[3].current_r = red4;
    led_gradient[3].current_g = green4;
    led_gradient[3].current_b = blue4;
}

void deng_clear()
{
	red4_zhong = 0,red2_zhong = 0,red3_zhong = 0,green4_zhong = 0,green2_zhong = 0,green3_zhong = 0,blue4_zhong = 0,blue2_zhong = 0,blue3_zhong = 0,red1_zhong=0,green1_zhong=0,blue1_zhong=0;
}
void deng_update()
{
	red4 = red4_zhong,red2 = red2_zhong,red3 = red3_zhong,red1 = red1_zhong,green4 = green4_zhong,green2 = green2_zhong,green3 = green3_zhong,green1 = green1_zhong,blue4 = blue4_zhong,blue2 = blue2_zhong,blue3 = blue3_zhong,blue1 = blue1_zhong;
	red4_old = red4_zhong,red2_old = red2_zhong,red3_old = red3_zhong,red1_old = red1_zhong,green4_old = green4_zhong,green2_old = green2_zhong,green3_old = green3_zhong,green1_old = green1_zhong,blue4_old = blue4_zhong,blue2_old = blue2_zhong,blue3_old = blue3_zhong,blue1_old = blue1_zhong;
	WriteRead[0] = red1  ,WriteRead[1] = red2  ,WriteRead[2] = red3  ,WriteRead[3] = red4  ;
	WriteRead[4] = green1,WriteRead[5] = green2,WriteRead[6] = green3,WriteRead[7] = green4;
	WriteRead[8] = blue1 ,WriteRead[9] = blue2 ,WriteRead[10] = blue3,WriteRead[11] = blue4;
	WriteRead[12] = Num,WriteRead[13] = bright,WriteRead[14] = deng_2or3;
	WriteRead[15] = volume;  // ✅ 保存音量到Flash
	W25Q128_EraseSector(0x002000);
	W25Q128_BufferWrite(WriteRead, 0x002000 , 16);  // ✅ 写入16字节
}
void deng_init()
{
	W25Q128_BufferRead(WriteRead, 0x002000 , 16);  // ✅ 读取16字节
	red1   = WriteRead[0],red2 =   WriteRead[1],red3 =   WriteRead[2],red4 =   WriteRead[3];
	green1 = WriteRead[4],green2 = WriteRead[5],green3 = WriteRead[6],green4 = WriteRead[7];
	blue1 =  WriteRead[8],blue2 =  WriteRead[9],blue3 = WriteRead[10],blue4 = WriteRead[11];
	bright = WriteRead[13],deng_2or3 = WriteRead[14];
	
	// ✅ 读取音量，如果为0xFF（未初始化）则使用默认值80
	volume = WriteRead[15];
	if(volume > 100 || volume == 0xFF) volume = 80;
	volume_old = volume;
	
	// 风扇速度默认为0，不从Flash读取
	Num = 0;
	current_speed_kmh = 0;  // ✅ 精度修复：同步初始化绝对速度
	red4_old = red4_zhong = red4,red2_old = red2_zhong = red2,red3_old = red3_zhong = red3,red1_old = red1_zhong = red1,
	green4_old = green4_zhong = green4,green2_old = green2_zhong = green2,green3_old = green3_zhong = green3,green1_old = green1_zhong = green1,
	blue4_old = blue4_zhong = blue4,blue2_old = blue2_zhong = blue2,blue3_old = blue3_zhong = blue3,blue1_old = blue1_zhong= blue1;
}
void deng_zi()
{
	if(name != name_old)
	{
		Fill_Circle(deng_qiu_1_x,deng_qiu_1_y,17,red_tiao_color);
		Fill_Circle(deng_qiu_2_x,deng_qiu_2_y,17,green_tiao_color);
		Fill_Circle(deng_qiu_3_x,deng_qiu_3_y,17,blue_tiao_color);
	}
	if(name == 0)//刷新数字
	{
		if((red1_old>=100&&red1_zhong<100))
		{
			Fill_Circle(deng_qiu_1_x,deng_qiu_1_y,17,red_tiao_color);
		}
		else if((red1_old>=10&&red1_zhong<10))
		{
			Fill_Circle(deng_qiu_1_x,deng_qiu_1_y,17,red_tiao_color);
		}
		if(red1_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,red_zi_y,red1_zhong,1,WHITE,red_tiao_color,16);
		}
		else if(red1_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,red_zi_y,red1_zhong,2,WHITE,red_tiao_color,16);
		}
		else if(red1_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,red_zi_y,red1_zhong,3,WHITE,red_tiao_color,16);
		}
		
		
		if((green1_old>=100&&green1_zhong<100))
		{
			Fill_Circle(deng_qiu_2_x,deng_qiu_2_y,17,green_tiao_color);
		}
		else if((green1_old>=10&&green1_zhong<10))
		{
			Fill_Circle(deng_qiu_2_x,deng_qiu_2_y,17,green_tiao_color);
		}
		if(green1_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,green_zi_y,green1_zhong,1,WHITE,green_tiao_color,16);
		}
		else if(green1_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,green_zi_y,green1_zhong,2,WHITE,green_tiao_color,16);
		}
		else if(green1_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,green_zi_y,green1_zhong,3,WHITE,green_tiao_color,16);
		}
		
		
		if((blue1_old>=100&&blue1_zhong<100))
		{
			Fill_Circle(deng_qiu_3_x,deng_qiu_3_y,17,blue_tiao_color);
		}
		else if((blue1_old>=10&&blue1_zhong<10))
		{
			Fill_Circle(deng_qiu_3_x,deng_qiu_3_y,17,blue_tiao_color);
		}
		if(blue1_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,blue_zi_y,blue1_zhong,1,WHITE,blue_tiao_color,16);
		}
		else if(blue1_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,blue_zi_y,blue1_zhong,2,WHITE,blue_tiao_color,16);
		}
		else if(blue1_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,blue_zi_y,blue1_zhong,3,WHITE,blue_tiao_color,16);
		}
	}
	if(name == 1)
	{
		if((red2_old>=100&&red2_zhong<100))
		{
			Fill_Circle(deng_qiu_1_x,deng_qiu_1_y,17,red_tiao_color);
		}
		else if((red2_old>=10&&red2_zhong<10))
		{
			Fill_Circle(deng_qiu_1_x,deng_qiu_1_y,17,red_tiao_color);
		}
		if(red2_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,red_zi_y,red2_zhong,1,WHITE,red_tiao_color,16);
		}
		else if(red2_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,red_zi_y,red2_zhong,2,WHITE,red_tiao_color,16);
		}
		else if(red2_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,red_zi_y,red2_zhong,3,WHITE,red_tiao_color,16);
		}
		
		
		if((green2_old>=100&&green2_zhong<100))
		{
			Fill_Circle(deng_qiu_2_x,deng_qiu_2_y,17,green_tiao_color);
		}
		else if((green2_old>=10&&green2_zhong<10))
		{
			Fill_Circle(deng_qiu_2_x,deng_qiu_2_y,17,green_tiao_color);
		}
		if(green2_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,green_zi_y,green2_zhong,1,WHITE,green_tiao_color,16);
		}
		else if(green2_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,green_zi_y,green2_zhong,2,WHITE,green_tiao_color,16);
		}
		else if(green2_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,green_zi_y,green2_zhong,3,WHITE,green_tiao_color,16);
		}
		
		
		if((blue2_old>=100&&blue2_zhong<100))
		{
			Fill_Circle(deng_qiu_3_x,deng_qiu_3_y,17,blue_tiao_color);
		}
		else if((blue2_old>=10&&blue2_zhong<10))
		{
			Fill_Circle(deng_qiu_3_x,deng_qiu_3_y,17,blue_tiao_color);
		}
		if(blue2_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,blue_zi_y,blue2_zhong,1,WHITE,blue_tiao_color,16);
		}
		else if(blue2_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,blue_zi_y,blue2_zhong,2,WHITE,blue_tiao_color,16);
		}
		else if(blue2_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,blue_zi_y,blue2_zhong,3,WHITE,blue_tiao_color,16);
		}
	}
	if(name == 2)
	{
		if((red3_old>=100&&red3_zhong<100))
		{
			Fill_Circle(deng_qiu_1_x,deng_qiu_1_y,17,red_tiao_color);
		}
		else if((red3_old>=10&&red3_zhong<10))
		{
			Fill_Circle(deng_qiu_1_x,deng_qiu_1_y,17,red_tiao_color);
		}
		if(red3_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,red_zi_y,red3_zhong,1,WHITE,red_tiao_color,16);
		}
		else if(red3_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,red_zi_y,red3_zhong,2,WHITE,red_tiao_color,16);
		}
		else if(red3_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,red_zi_y,red3_zhong,3,WHITE,red_tiao_color,16);
		}
		
		
		if((green3_old>=100&&green3_zhong<100))
		{
			Fill_Circle(deng_qiu_2_x,deng_qiu_2_y,17,green_tiao_color);
		}
		else if((green3_old>=10&&green3_zhong<10))
		{
			Fill_Circle(deng_qiu_2_x,deng_qiu_2_y,17,green_tiao_color);
		}
		if(green3_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,green_zi_y,green3_zhong,1,WHITE,green_tiao_color,16);
		}
		else if(green3_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,green_zi_y,green3_zhong,2,WHITE,green_tiao_color,16);
		}
		else if(green3_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,green_zi_y,green3_zhong,3,WHITE,green_tiao_color,16);
		}
		
		
		if((blue3_old>=100&&blue3_zhong<100))
		{
			Fill_Circle(deng_qiu_3_x,deng_qiu_3_y,17,blue_tiao_color);
		}
		else if((blue3_old>=10&&blue3_zhong<10))
		{
			Fill_Circle(deng_qiu_3_x,deng_qiu_3_y,17,blue_tiao_color);
		}
		if(blue3_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,blue_zi_y,blue3_zhong,1,WHITE,blue_tiao_color,16);
		}
		else if(blue3_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,blue_zi_y,blue3_zhong,2,WHITE,blue_tiao_color,16);
		}
		else if(blue3_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,blue_zi_y,blue3_zhong,3,WHITE,blue_tiao_color,16);
		}
	}
	if(name == 3)
	{
		if((red4_old>=100&&red4_zhong<100))
		{
			Fill_Circle(deng_qiu_1_x,deng_qiu_1_y,17,red_tiao_color);
		}
		else if((red4_old>=10&&red4_zhong<10))
		{
			Fill_Circle(deng_qiu_1_x,deng_qiu_1_y,17,red_tiao_color);
		}
		if(red4_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,red_zi_y,red4_zhong,1,WHITE,red_tiao_color,16);
		}
		else if(red4_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,red_zi_y,red4_zhong,2,WHITE,red_tiao_color,16);
		}
		else if(red4_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,red_zi_y,red4_zhong,3,WHITE,red_tiao_color,16);
		}
		
		
		if((green4_old>=100&&green4_zhong<100))
		{
			Fill_Circle(deng_qiu_2_x,deng_qiu_2_y,17,green_tiao_color);
		}
		else if((green4_old>=10&&green4_zhong<10))
		{
			Fill_Circle(deng_qiu_2_x,deng_qiu_2_y,17,green_tiao_color);
		}
		if(green4_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,green_zi_y,green4_zhong,1,WHITE,green_tiao_color,16);
		}
		else if(green4_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,green_zi_y,green4_zhong,2,WHITE,green_tiao_color,16);
		}
		else if(green4_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,green_zi_y,green4_zhong,3,WHITE,green_tiao_color,16);
		}
		
		
		if((blue4_old>=100&&blue4_zhong<100))
		{
			Fill_Circle(deng_qiu_3_x,deng_qiu_3_y,17,blue_tiao_color);
		}
		else if((blue4_old>=10&&blue4_zhong<10))
		{
			Fill_Circle(deng_qiu_3_x,deng_qiu_3_y,17,blue_tiao_color);
		}
		if(blue4_zhong<10)
		{
			LCD_ShowIntNum(one_zi_x,blue_zi_y,blue4_zhong,1,WHITE,blue_tiao_color,16);
		}
		else if(blue4_zhong<100)
		{
			LCD_ShowIntNum(two_zi_x,blue_zi_y,blue4_zhong,2,WHITE,blue_tiao_color,16);
		}
		else if(blue4_zhong>=100)
		{
			LCD_ShowIntNum(thr_zi_x,blue_zi_y,blue4_zhong,3,WHITE,blue_tiao_color,16);
		}
	}
}

void tiao_se()
{
	// ═══════════════════════════════════════════════════════════════════
	// 🎨 UI3 RGB调色 - 三层状态机（丝滑+防跳版）
	// ═══════════════════════════════════════════════════════════════════
	// 状态机：
	//   ui3_mode = 0: 选择灯带模式 (旋转切换 Middle/Left/Right/Back) - 红点
	//   ui3_mode = 1: 选择通道模式 (旋转切换 R/G/B) - 绿点
	//   ui3_mode = 2: 调节数值模式 (旋转调节0-255) - 绿点+高亮
	// 
	// 操作说明：
	//   选灯带模式: 旋转切换灯带，单击进入选通道
	//   选通道模式: 旋转切换R/G/B，单击进入调节，长按退出回选灯带
	//   调节模式:   旋转调数值，单击退出回选通道
	// ═══════════════════════════════════════════════════════════════════
	
	// 防跳机制：时间间隔限制
	static uint32_t last_switch_tick = 0;
	
	int16_t delta = encoder_delta;
	
	// ═══════════════════════════════════════════════════════════════════
	// 模式0：选择灯带 (红点) - 旋转切换 Middle/Left/Right/Back
	// ═══════════════════════════════════════════════════════════════════
	if(ui3_mode == 0)
	{
		// 防跳：至少间隔150ms才能切换一次
		if(delta != 0 && uwTick - last_switch_tick >= 150)
		{
			if(delta > 0)
			{
				name++;
				if(name > 3) name = 0;
			}
			else
			{
				name--;
				if(name < 0) name = 3;
			}
			last_switch_tick = uwTick;
			
			// 更新LCD显示
			lcd_name_update(name);
			lcd_rgb_update(4);
			lcd_rgb_update(5);
			lcd_rgb_update(6);
			lcd_deng(1, name);  // 1=红点(选灯带模式)
		}
		name_old = name;
	}
	// ═══════════════════════════════════════════════════════════════════
	// 模式1：选择通道 R/G/B (绿点) - 旋转切换通道
	// ═══════════════════════════════════════════════════════════════════
	else if(ui3_mode == 1)
	{
		// 防跳：至少间隔150ms
		if(delta != 0 && uwTick - last_switch_tick >= 150)
		{
			if(delta > 0)
			{
				ui3_channel++;
				if(ui3_channel > 2) ui3_channel = 0;
			}
			else
			{
				ui3_channel--;
				if(ui3_channel < 0 || ui3_channel > 2) ui3_channel = 2;
			}
			last_switch_tick = uwTick;
			
			// 更新LCD高亮显示当前通道
			lcd_rgb_update(4);
			lcd_rgb_update(5);
			lcd_rgb_update(6);
			lcd_rgb_cai_update(ui3_channel + 4);  // 高亮当前通道
			lcd_deng(0, name);  // 0=绿点(选通道模式)
		}
	}
	// ═══════════════════════════════════════════════════════════════════
	// 模式2：调节RGB数值 (绿点+高亮) - 旋转调节0-255
	// ═══════════════════════════════════════════════════════════════════
	else if(ui3_mode == 2)
	{
		// 调节数值不需要防跳，但需要限制刷新频率
		if(delta != 0)
		{
			int16_t *target_value = NULL;
			
			// 选择目标变量
			if(ui3_channel == 0)  // R通道
			{
				switch(name) {
					case 0: target_value = &red1_zhong; break;
					case 1: target_value = &red2_zhong; break;
					case 2: target_value = &red3_zhong; break;
					case 3: target_value = &red4_zhong; break;
				}
			}
			else if(ui3_channel == 1)  // G通道
			{
				switch(name) {
					case 0: target_value = &green1_zhong; break;
					case 1: target_value = &green2_zhong; break;
					case 2: target_value = &green3_zhong; break;
					case 3: target_value = &green4_zhong; break;
				}
			}
			else if(ui3_channel == 2)  // B通道
			{
				switch(name) {
					case 0: target_value = &blue1_zhong; break;
					case 1: target_value = &blue2_zhong; break;
					case 2: target_value = &blue3_zhong; break;
					case 3: target_value = &blue4_zhong; break;
				}
			}
			
			if(target_value != NULL)
			{
				// 调节值（乘2加速）
				*target_value += delta * 2;
				
				// 限制范围 0-255
				if(*target_value > 255) *target_value = 255;
				if(*target_value < 0) *target_value = 0;
				
				// 获取当前灯带的RGB值
				int16_t r, g, b;
				switch(name) {
					case 0: r = red1_zhong; g = green1_zhong; b = blue1_zhong; break;
					case 1: r = red2_zhong; g = green2_zhong; b = blue2_zhong; break;
					case 2: r = red3_zhong; g = green3_zhong; b = blue3_zhong; break;
					case 3: r = red4_zhong; g = green4_zhong; b = blue4_zhong; break;
					default: r = g = b = 0; break;
				}
				
				// 实时更新LED
				WS2812B_SetAllLEDs(name + 1, r * bright * bright_num, g * bright * bright_num, b * bright * bright_num);
				WS2812B_Update(name + 1);
				
				// 更新LCD数字显示
				lcd_rgb_num(ui3_channel + 4, *target_value);
			}
		}
	}
}

void deng_ui2()
{
	// 12条颜色预设配置（与APP端和rx.c保持一致）
	if(deng_num == 1)  // 赛博霓虹 - 蓝紫→春绿
	{
		red2_zhong = 138,green2_zhong = 43,blue2_zhong = 226;
		red3_zhong = 0,green3_zhong = 255,blue3_zhong = 128;
	}
	else if(deng_num == 2)  // 冰晶青 - 青色纯色
	{
		red2_zhong = 0,green2_zhong = 234,blue2_zhong = 255;
		red3_zhong = 0,green3_zhong = 234,blue3_zhong = 255;
	}
	else if(deng_num == 3)  // 日落熔岩 - 橙红→天青
	{
		red2_zhong = 255,green2_zhong = 100,blue2_zhong = 0;
		red3_zhong = 0,green3_zhong = 200,blue3_zhong = 255;
	}
	else if(deng_num == 4)  // 竞速黄 - 黄色纯色
	{
		red2_zhong = 255,green2_zhong = 210,blue2_zhong = 0;
		red3_zhong = 255,green3_zhong = 210,blue3_zhong = 0;
	}
	else if(deng_num == 5)  // 烈焰红 - 红色纯色
	{
		red2_zhong = 255,green2_zhong = 0,blue2_zhong = 0;
		red3_zhong = 255,green3_zhong = 0,blue3_zhong = 0;
	}
	else if(deng_num == 6)  // 警灯双闪 - 红→警蓝
	{
		red2_zhong = 255,green2_zhong = 0,blue2_zhong = 0;
		red3_zhong = 0,green3_zhong = 80,blue3_zhong = 255;
	}
	else if(deng_num == 7)  // 樱花绯红 - 热粉→玫红
	{
		red2_zhong = 255,green2_zhong = 105,blue2_zhong = 180;
		red3_zhong = 255,green3_zhong = 0,blue3_zhong = 80;
	}
	else if(deng_num == 8)  // 极光幻紫 - 紫→青绿
	{
		red2_zhong = 180,green2_zhong = 0,blue2_zhong = 255;
		red3_zhong = 0,green3_zhong = 255,blue3_zhong = 200;
	}
	else if(deng_num == 9)  // 暗夜紫晶 - 紫色纯色
	{
		red2_zhong = 148,green2_zhong = 0,blue2_zhong = 211;
		red3_zhong = 148,green3_zhong = 0,blue3_zhong = 211;
	}
	else if(deng_num == 10)  // 薄荷清风 - 薄荷→天蓝
	{
		red2_zhong = 0,green2_zhong = 255,blue2_zhong = 180;
		red3_zhong = 100,green3_zhong = 200,blue3_zhong = 255;
	}
	else if(deng_num == 11)  // 丛林猛兽 - 荧光绿纯色
	{
		red2_zhong = 0,green2_zhong = 255,blue2_zhong = 65;
		red3_zhong = 0,green3_zhong = 255,blue3_zhong = 65;
	}
	else if(deng_num == 12)  // 纯净白 - 暖白纯色
	{
		red2_zhong = 225,green2_zhong = 225,blue2_zhong = 225;
		red3_zhong = 225,green3_zhong = 225,blue3_zhong = 225;
	}
	else if(deng_num == 13)  // 烈焰橙 - 深橙→浅橙
	{
		red2_zhong = 255,green2_zhong = 80,blue2_zhong = 0;
		red3_zhong = 255,green3_zhong = 200,blue3_zhong = 50;
	}
	else if(deng_num == 14)  // 霓虹派对 - 青→品红
	{
		red2_zhong = 0,green2_zhong = 255,blue2_zhong = 255;
		red3_zhong = 255,green3_zhong = 0,blue3_zhong = 255;
	}
	if(deng_num!=deng_num_old)
	{
		// ✅ 同步 LED1 和 LED4 的颜色（LED1=LED2颜色，LED4=LED3颜色）
		red1_zhong = red2_zhong; green1_zhong = green2_zhong; blue1_zhong = blue2_zhong;
		red4_zhong = red3_zhong; green4_zhong = green3_zhong; blue4_zhong = blue3_zhong;
		
		WS2812B_Set23LEDs(2,red2_zhong*bright*bright_num,green2_zhong*bright*bright_num,blue2_zhong*bright*bright_num);
		WS2812B_Set23LEDs(3,red3_zhong*bright*bright_num,green3_zhong*bright*bright_num,blue3_zhong*bright*bright_num);
		WS2812B_Update(1);
	}
	deng_num_old = deng_num;
}
// 呼吸灯颜色预设索引（用于呼吸灯模式旋转切换）
static uint8_t breath_color_index = 1;  // 1-14

// 呼吸灯12种颜色预设（与UI2配色界面一致）
static const uint8_t breath_colors[14][6] = {
    // {R2, G2, B2, R3, G3, B3} - 左侧颜色, 右侧颜色
    {138, 43, 226, 0, 255, 128},    // 1. 赛博霓虹 - 蓝紫→春绿
    {0, 234, 255, 0, 234, 255},     // 2. 冰晶青 - 青色纯色
    {255, 100, 0, 0, 200, 255},     // 3. 日落熔岩 - 橙红→天青
    {255, 210, 0, 255, 210, 0},     // 4. 竞速黄 - 黄色纯色
    {255, 0, 0, 255, 0, 0},         // 5. 烈焰红 - 红色纯色
    {255, 0, 0, 0, 80, 255},        // 6. 警灯双闪 - 红→警蓝
    {255, 105, 180, 255, 0, 80},    // 7. 樱花绯红 - 热粉→玫红
    {180, 0, 255, 0, 255, 200},     // 8. 极光幻紫 - 紫→青绿
    {148, 0, 211, 148, 0, 211},     // 9. 暗夜紫晶 - 紫色纯色
    {0, 255, 180, 100, 200, 255},   // 10. 薄荷清风 - 薄荷→天蓝
    {0, 255, 65, 0, 255, 65},       // 11. 丛林猛兽 - 荧光绿纯色
    {225, 225, 225, 225, 225, 225}, // 12. 纯净白 - 暖白纯色
    {255, 80, 0, 255, 200, 50},     // 13. 烈焰橙 - 深橙→浅橙
    {0, 255, 255, 255, 0, 255}      // 14. 霓虹派对 - 青→品红
};

void deng_ui4()
{
	// ═══════════════════════════════════════════════════════════════════
	// 🌟 UI4 亮度调节 + 呼吸灯功能
	// ═══════════════════════════════════════════════════════════════════
	// breath_mode = 0: 正常模式(红点) - 旋转调节亮度
	// breath_mode = 1: 呼吸灯模式(绿点) - 旋转切换颜色 + LED自动呼吸 + 颜色条显示
	// ═══════════════════════════════════════════════════════════════════
	
	// 防跳机制
	static uint32_t last_color_switch_tick = 0;
	
	if(breath_mode == 0)
	{
		// 正常模式：旋转调节亮度
		bright += encoder_delta;
		if (bright >= 100) {
			bright = 100;
		} else if (bright <= 0) {
			bright = 0;
		}
		LCD_ui4_num_update(bright);
		if(bright != bright_old)
		{
			deng();
		}
		bright_old = bright;
	}
	else
	{
		// ═══════════════════════════════════════════════════════════════════
		// 呼吸灯模式：旋转切换颜色预设 + LED自动呼吸 + 颜色条可视化
		// ═══════════════════════════════════════════════════════════════════
		
		// 旋转切换颜色预设（防跳：150ms间隔）
		if(encoder_delta != 0 && uwTick - last_color_switch_tick >= 150)
		{
			if(encoder_delta > 0)
			{
				breath_color_index++;
				if(breath_color_index > 14) breath_color_index = 1;
			}
			else
			{
				breath_color_index--;
				if(breath_color_index < 1 || breath_color_index > 14) breath_color_index = 14;
			}
			last_color_switch_tick = uwTick;
			
			// 更新颜色到全局变量（用于颜色条显示）
			uint8_t idx = breath_color_index - 1;
			red2_zhong = breath_colors[idx][0];
			green2_zhong = breath_colors[idx][1];
			blue2_zhong = breath_colors[idx][2];
			red3_zhong = breath_colors[idx][3];
			green3_zhong = breath_colors[idx][4];
			blue3_zhong = breath_colors[idx][5];
		}
		
		// 每20ms更新一次呼吸效果 (50fps)
		if(uwTick - breath_tick >= 20)
		{
			breath_tick = uwTick;
			
			// 相位递增 (0-628 对应 0-2π)
			breath_phase += 4;  // 约3秒一个周期
			if(breath_phase >= 628) breath_phase = 0;
			
			// 使用正弦函数计算亮度 (0.2 ~ 1.0)
			float sin_val = sinf(breath_phase * 0.01f);
			float breath_factor = 0.6f + 0.4f * sin_val;
			
			// 计算当前呼吸亮度因子 (0-255)
			uint8_t breath_bright_factor = (uint8_t)(breath_factor * 255);
			
			// ✅ 使用颜色条显示当前颜色+呼吸效果
			LCD_DrawBreathBarGradient(breath_bright_factor);
			
			// 计算当前呼吸亮度
			uint8_t breath_bright = (uint8_t)(bright * breath_factor);
			float br = breath_bright * bright_num;
			
			// 更新LED（使用当前选中的颜色预设）
			WS2812B_SetAllLEDs(1, red1 * br, green1 * br, blue1 * br);
			WS2812B_SetAllLEDs(2, red2_zhong * br, green2_zhong * br, blue2_zhong * br);
			WS2812B_SetAllLEDs(3, red3_zhong * br, green3_zhong * br, blue3_zhong * br);
			WS2812B_Update(3);
			WS2812B_SetAllLEDs(4, red4 * br, green4 * br, blue4 * br);
			WS2812B_Update(4);
		}
	}
	bright_wid_old = bright_wid;
}

// ✅ 音量调节处理函数 (UI6)
u8 volume_state = 0;  // 音量调节状态（已废弃，保留兼容）

void volume_ui6(void)
{
	// ✅ 使用统一的 encoder_delta 调节音量
	volume += encoder_delta;
	if (volume > 100) {
		volume = 100;
	} else if (volume < 0 || volume > 200) {  // 防止下溢
		volume = 0;
	}
	
	// 更新LCD显示（只显示大数字）
	LCD_ui6_num_update(volume);
	
	// 音量变化时更新实际音量
	if(volume != volume_old)
	{
		VS1003_SetVolumePercent(volume);
		volume_old = volume;
	}
}

// 编码器计数处理变量
u32 encoder_process_tick = 0;  // 编码器处理时间戳

// ✅ 优化：简化编码器检测，移除不稳定的快速旋转检测
// 使用防抖动机制，确保方向判断准确
static uint32_t last_encoder_tick = 0;  // 上次编码器中断时间
static uint8_t last_direction = 0;      // 上次方向（用于连续性检测）

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // ✅ 已弃用：不再使用中断更新编码器计数
    // 改为在 Encoder_GetDelta() 中直接读取 TIM1->CNT
    // 保留此函数以避免链接错误
    (void)htim;
}




u32 pwm_tick = 0;

void PWM()
{
	if(uwTick - pwm_tick < 100)
		return;
	pwm_tick = uwTick;
	if(ui == 1)
	{
		// Num 范围 0-100，线性映射到 PWM 0-1000
		// 恢复简单的线性映射，低速啸叫是硬件问题，软件无法完全解决
		uint16_t pwm_value = Num * 10;
		__HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_1, pwm_value);
		__HAL_TIM_SetCompare(&htim3, TIM_CHANNEL_4, pwm_value);
	}
 }

u8 num_old = 0;
int16_t last_reported_speed = -1;  // 🏎️ 上次上报的速度值（用于检测变化）- 改为全局变量供 rx.c 访问

void Encoder()
{
	// 周期控制（20ms一次）
    if (uwTick - encoder_process_tick < 20) {
        return;
    }
    encoder_process_tick = uwTick;

    // ✅ 菜单界面重构：统一读取编码器增量到全局变量
    encoder_delta = Encoder_GetDelta();

    // ✅ 菜单界面重构：菜单界面的编码器处理 (ui=5)
    // 添加防抖机制，避免旋转一格触发多次切换
    if (ui == 5) {
        static uint32_t last_menu_switch_tick = 0;
        static int8_t accumulated_delta = 0;  // 累积增量
        
        // 累积编码器增量
        accumulated_delta += encoder_delta;
        
        // 防抖：至少间隔150ms才能切换一次页面
        if (uwTick - last_menu_switch_tick >= 150) {
            if (accumulated_delta >= 2) {  // 累积足够的正向增量才切换
                menu_selected++;
                if (menu_selected > 6) menu_selected = 1;
                last_menu_switch_tick = uwTick;
                accumulated_delta = 0;
            } else if (accumulated_delta <= -2) {  // 累积足够的负向增量才切换
                menu_selected--;
                if (menu_selected < 1) menu_selected = 6;
                last_menu_switch_tick = uwTick;
                accumulated_delta = 0;
            }
        }
        
        // 防止累积值过大
        if (accumulated_delta > 10) accumulated_delta = 10;
        if (accumulated_delta < -10) accumulated_delta = -10;
    }

    // 仅在界面1且非油门模式时调整Num值
    if (ui == 1 && wuhuaqi_state != 2) {
        // ✅ 使用统一的 encoder_delta 而非重复调用 Encoder_GetDelta()
        int16_t delta = encoder_delta;
        
        // ✅ 移除 KNOB 增量上报，只使用 SPEED_REPORT 绝对值
        // 原因：同时发送 KNOB 和 SPEED_REPORT 会导致 APP 端混乱
        
        Num += delta;
        if (Num > 100) Num = 100;
        if (Num < 0) Num = 0;
        
        // 🏎️ 当 Num 值变化时，上报绝对速度到 APP
        // 将 Num (0-100) 映射到速度 (0-340)
        // ✅ 精度修复：使用四舍五入 (+50 实现四舍五入)
        int16_t current_speed = (Num * 340 + 50) / 100;
        if (current_speed > 340) current_speed = 340;
        
        // ✅ 精度修复：同步更新绝对速度变量，确保 LCD 显示与 APP 一致
        if (delta != 0) {
            current_speed_kmh = current_speed;
        }
        
        if (current_speed != last_reported_speed && delta != 0) {
            BLE_ReportSpeed(current_speed, speed_value);
            last_reported_speed = current_speed;
        }
    }
    
    // 读取按键状态
    uint8_t key_now = (HAL_GPIO_ReadPin(ENC_PORT, ENC_KEY_PIN) == 0) ? 1 : 0;
    
    // 油门模式：按住加速，松开减速，旋转编码器退出
    if (wuhuaqi_state == 2) {
        static uint32_t last_throttle_adjust = 0;
        static uint32_t last_volume_adjust = 0;   // 音量调整时间（用于渐变）
        static uint8_t throttle_initialized = 0;  // 油门模式初始化标志
        static uint8_t key_old_throttle = 0;      // 油门模式内的按键状态记录
        static uint8_t current_volume = 0;        // 当前音量（用于平滑过渡）
        static uint8_t was_remote_controlled = 0; // 标记是否刚从远程模式退出
        static int16_t frozen_num = -1;           // 远程模式退出时冻结的速度值
        
        // 首次进入油门模式时初始化
        if (!throttle_initialized) {
            throttle_initialized = 1;
            key_old_throttle = 0;
            current_volume = 0;
            was_remote_controlled = 0;
            frozen_num = -1;
            last_throttle_adjust = uwTick;
            last_volume_adjust = uwTick;
        }
        
        // ════════════════════════════════════════════════════════════
        // 油门模式退出：只有旋转编码器才能退出，退出后保持在调速界面
        // ════════════════════════════════════════════════════════════
        
        // 检测编码器旋转 - 任意方向旋转都退出油门模式
        int16_t delta = encoder_delta;
        if (delta != 0) {
            // 旋转编码器退出油门模式
            wuhuaqi_state = wuhuaqi_state_saved;
            wuhuaqi_state_old = wuhuaqi_state;
            throttle_initialized = 0;
            was_remote_controlled = 0;
            frozen_num = -1;
            if (wuhuaqi_state == 0) {
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
            } else {
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
            }
            // 退出油门模式后保持在调速界面，用户可以继续用旋钮调节风速
            ui = 1;
            chu = 2;  // 保持UI1已初始化状态
            lcd_wuhuaqi(wuhuaqi_state, speed_value);
            EngineAudio_Stop();
            BLE_ReportThrottle(0);
            key_old = key_now;
            return;
        }
        
        // 检测按下边沿（记录时间，用于长按检测等）
        if (key_now == 1 && key_old == 0) {
            key_tick = uwTick;
        }
        
        // 更新 key_old
        key_old = key_now;
        
        // ════════════════════════════════════════════════════════════
        // 以下是加速/减速逻辑，受远程控制影响
        // ════════════════════════════════════════════════════════════
        
        // 🔊 音量映射：一开始就要有声音，然后逐渐变大
        // 速度 0-340 映射到音量 70-100（初始就比较响）
        uint8_t target_volume;
        if (current_speed_kmh > 0) {
            // 基础音量70% + 速度贡献30%（一开始就响）
            target_volume = 70 + (current_speed_kmh * 30 / 340);
            if (target_volume > 100) target_volume = 100;
        } else {
            target_volume = 0;
        }
        
        // 按住加速
        extern uint32_t remote_active_tick;
        
        // 远程控制模式的状态机（只影响加速/减速）
        if (uwTick - remote_active_tick < 500) {
            // 【远程控制中】本地按键和自动减速逻辑全部失效，Num 完全由蓝牙接管
            was_remote_controlled = 1;  // 标记为远程控制状态
            frozen_num = Num;           // 持续记录当前速度（用于退出时冻结）
            key_old_throttle = key_now; 
            last_throttle_adjust = uwTick;
            return;  // 直接返回，不执行加速/减速逻辑
        } 
        else if (was_remote_controlled) {
            // 【刚从远程模式退出】冻结当前值，等待用户本地操作
            // 只有当用户按下本地按键时，才解除冻结状态
            if (key_now == 1 && key_old_throttle == 0) {
                // 用户按下了本地按键，解除冻结，恢复正常本地控制
                was_remote_controlled = 0;
                frozen_num = -1;
                EngineAudio_Start();
                current_volume = 70;  // 🔊 初始音量70%
                EngineAudio_SetVolume(current_volume);
            } else {
                // 保持冻结状态，不执行加减速
                key_old_throttle = key_now;
                return;  // 直接返回，保持当前速度不变
            }
        }
        
        // ===== 以下是正常的本地油门控制逻辑 =====
        static int16_t last_throttle_speed = -1;  // 油门模式下上次上报的速度
        static uint32_t last_speed_report_tick = 0;  // 上次上报速度的时间
        
        if (key_now == 1) {
            // 检测按下边沿 - 立即开始播放音频
            if (key_old_throttle == 0) {
                EngineAudio_Start();  // 按下瞬间立即启动音频
                current_volume = 70;  // 🔊 初始音量70%，一开始就能听到
                EngineAudio_SetVolume(current_volume);
            }
            
            // 🏎️ 丝滑加速：直接操作 current_speed_kmh，每18ms增加1
            // 从0加速到340需要约6秒（340 * 18ms = 6.12秒）
            if (uwTick - last_throttle_adjust >= 18) {
                if (current_speed_kmh < 340) {
                    current_speed_kmh += 1;  // 每次只增加1，丝滑变化
                }
                // 反向计算 Num（用于PWM和音量控制）
                Num = (current_speed_kmh * 100 + 170) / 340;  // 四舍五入
                if (Num > 100) Num = 100;
                last_throttle_adjust = uwTick;
                
                // 上报速度到APP（每100ms或达到极值时上报）
                if (current_speed_kmh == 340 || uwTick - last_speed_report_tick >= 100) {
                    BLE_ReportSpeed(current_speed_kmh, speed_value);
                    last_throttle_speed = current_speed_kmh;
                    last_speed_report_tick = uwTick;
                }
            }
            
            // 🔊 音量渐变跟随目标值（每次调整2%，平滑过渡）
            if (current_volume < target_volume) {
                current_volume += 2;  // 渐变增加
                if (current_volume > target_volume) current_volume = target_volume;
                EngineAudio_SetVolume(current_volume);
            } else if (current_volume > target_volume) {
                current_volume -= 1;  // 减小时更慢
                if (current_volume < target_volume || current_volume > 200) current_volume = target_volume;
                EngineAudio_SetVolume(current_volume);
            }
        }
        // 松开减速 - 只有在非冻结状态下才执行减速
        else if (!was_remote_controlled) {
            // 🏎️ 丝滑减速：直接操作 current_speed_kmh，每12ms减少1
            // 从340减到0需要约4秒（340 * 12ms = 4.08秒），有惯性滑行感
            if (uwTick - last_throttle_adjust >= 12) {
                if (current_speed_kmh > 0) {
                    current_speed_kmh -= 1;  // 每次只减少1，丝滑变化
                }
                // 反向计算 Num（用于PWM和音量控制）
                Num = (current_speed_kmh * 100 + 170) / 340;  // 四舍五入
                if (Num < 0) Num = 0;
                last_throttle_adjust = uwTick;
                
                // 上报速度到APP（每100ms或达到极值时上报）
                if (current_speed_kmh == 0 || uwTick - last_speed_report_tick >= 100) {
                    BLE_ReportSpeed(current_speed_kmh, speed_value);
                    last_throttle_speed = current_speed_kmh;
                    last_speed_report_tick = uwTick;
                }
            }
            
            // 减速时音量缓慢减小（每30ms调整一次，模拟引擎惯性）
            if (uwTick - last_volume_adjust >= 30) {
                if (current_volume > target_volume) {
                    current_volume -= 2;  // 缓慢减小（比加速慢）
                    if (current_volume < target_volume || current_volume > 200) {  // 防止下溢
                        current_volume = target_volume;
                    }
                }
                
                if (current_volume > 0) {
                    EngineAudio_SetVolume(current_volume);
                } else {
                    // 音量归零时停止音频
                    if (EngineAudio_IsPlaying()) {
                        EngineAudio_Stop();
                    }
                }
                last_volume_adjust = uwTick;
            }
        }
        
        key_old_throttle = key_now;
        key_old = key_now;
        
        return;
    }
    
    // 非油门模式：正常按键检测
    // 检测按下边沿
    if (key_now == 1 && key_old == 0) {
        key_tick = uwTick;
        // ✅ 修复：按下时重置长按标志，准备新的按键周期
        if (key_state == 0xFF) {
            key_state = 0;
        }
    }
    
    // 检测松开边沿 - 短按（<400ms）
    // ✅ 修复：只有key_state不是0xFF（长按标记）时才处理短按
    if (key_now == 0 && key_old == 1 && uwTick - key_tick < 400 && key_state != 0xFF) {
        // 累加点击次数
        if (key_state == 0) {
            key_state = 1;
            key_state_tick = uwTick;
        } else if (uwTick - key_state_tick < 400) {
            // 在400ms内的连续点击
            key_state++;
            if (key_state > 3) key_state = 3;  // 最多3击
            key_state_tick = uwTick;
        } else {
            // 超时，重新开始
            key_state = 1;
            key_state_tick = uwTick;
        }
    }
    
    // ✅ 修复：长按释放时重置key_state
    if (key_now == 0 && key_old == 1 && key_state == 0xFF) {
        key_state = 0;  // 重置长按标志，准备下次按键
    }
    
    // ✅ 长按检测（800ms）
    // - 油门模式：长按=加速（已在油门模式逻辑中处理）
    // - 非油门模式且在UI1：长按=切换雾化器
    // - UI3选通道/调节模式：长按=退出回选灯带模式
    if (key_now == 1 && uwTick - key_tick >= 800) {
        // UI3 RGB调色界面：长按退出回选灯带模式
        if (ui == 3 && (ui3_mode == 1 || ui3_mode == 2)) {
            ui3_mode = 0;  // 退出回选灯带模式
            ui3_channel = 0;
            
            // 恢复选灯带模式显示
            lcd_rgb_update(4);
            lcd_rgb_update(5);
            lcd_rgb_update(6);
            lcd_deng(1, name);  // 1=红点(选灯带模式)
            
            // 标记长按已触发
            key_state = 0xFF;
        }
        // 非油门模式下，在UI1界面长按切换雾化器
        else if (wuhuaqi_state != 2 && ui == 1 && chu == 2) {
            // 切换雾化器状态（0 ↔ 1）
            if (wuhuaqi_state == 1) {
                // 关闭雾化器
                wuhuaqi_state = 0;
                wuhuaqi_state_old = 0;
                wuhuaqi_state_saved = 0;
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
            } else {
                // 开启雾化器
                wuhuaqi_state = 1;
                wuhuaqi_state_old = 1;
                wuhuaqi_state_saved = 1;
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
            }
            
            // 刷新LCD显示雾化器状态
            lcd_wuhuaqi(wuhuaqi_state, speed_value);
            
            // 🆕 上报按钮事件到APP（长按切换雾化器）
            BLE_ReportButtonEvent("KNOB", "LONG_PRESS");
            
            // ✅ 标记长按已触发，防止松开时误触发单击
            key_state = 0xFF;  // 使用特殊值标记长按已处理
        }
        // 油门模式：长按=加速（已在油门模式逻辑中处理，这里不需要额外代码）
        
        key_tick = uwTick;  // 重置计时，防止重复触发
    }
    
    key_old = key_now;
    
    // 超时400ms后执行点击操作
    // ✅ 修复：如果key_state=0xFF（长按已触发），则不执行点击操作
    if (key_state > 0 && key_state != 0xFF && uwTick - key_state_tick >= 400)
	{
		if(key_state == 1)//单击 - 切换单位（km/h ↔ mph）或进入子功能
		{
			// 🆕 上报按钮事件到APP
			BLE_ReportButtonEvent("KNOB", "CLICK");
			
			// ✅ 菜单界面重构：菜单界面单击进入选中的 UI
			if(ui == 5)
			{
				// Logo页面(menu_selected=5)进入ui=6，Voice页面(menu_selected=6)进入ui=7
				if(menu_selected == 5) {
					ui = 6;   // Logo界面
					chu = 6;
				} else if(menu_selected == 6) {
					ui = 7;   // ✅ 音量调节界面
					chu = 7;
				} else if(menu_selected <= 4) {
					ui = menu_selected;
					chu = menu_selected;
				}
			}
			else if(ui == 1 && chu == 2 && wuhuaqi_state != 2)  // 界面1，非油门模式
			{
				if(speed_value == 0)
				{
					speed_value = 1;
				}
				else
				{
					speed_value = 0;
				}
				lcd_wuhuaqi(wuhuaqi_state, speed_value);
				
				// ✅ 精度修复：使用绝对速度刷新数字显示，确保与 APP 端一致
				LCD_picture(current_speed_kmh, 2);
				
				// 🆕 上报单位切换到APP，实现实时同步
				BLE_ReportUnit(speed_value);
			}
			else if(ui == 3)
			{
				// ═══════════════════════════════════════════════════════════════════
				// 🎨 UI3 RGB调色界面 - 三层状态机单击处理
				// ═══════════════════════════════════════════════════════════════════
				// ui3_mode = 0: 选灯带模式 → 单击进入选通道模式
				// ui3_mode = 1: 选通道模式 → 单击进入调节模式，长按退出回选灯带
				// ui3_mode = 2: 调节模式 → 单击退出回到选通道模式
				// ═══════════════════════════════════════════════════════════════════
				
				if(ui3_mode == 0)
				{
					// 从选灯带模式 → 进入选通道模式
					ui3_mode = 1;
					ui3_channel = 0;  // 默认选中R通道
					
					// 更新LCD显示：高亮R通道，显示绿点
					lcd_rgb_cai_update(4);  // R高亮
					lcd_rgb_update(5);      // G普通
					lcd_rgb_update(6);      // B普通
					lcd_deng(0, name);      // 0=绿点(选通道模式)
				}
				else if(ui3_mode == 1)
				{
					// 从选通道模式 → 进入调节模式
					ui3_mode = 2;
					
					// 显示当前通道的数值
					int16_t current_value = 0;
					switch(ui3_channel) {
						case 0:  // R
							switch(name) {
								case 0: current_value = red1_zhong; break;
								case 1: current_value = red2_zhong; break;
								case 2: current_value = red3_zhong; break;
								case 3: current_value = red4_zhong; break;
							}
							lcd_rgb_num(4, current_value);
							break;
						case 1:  // G
							switch(name) {
								case 0: current_value = green1_zhong; break;
								case 1: current_value = green2_zhong; break;
								case 2: current_value = green3_zhong; break;
								case 3: current_value = green4_zhong; break;
							}
							lcd_rgb_num(5, current_value);
							break;
						case 2:  // B
							switch(name) {
								case 0: current_value = blue1_zhong; break;
								case 1: current_value = blue2_zhong; break;
								case 2: current_value = blue3_zhong; break;
								case 3: current_value = blue4_zhong; break;
							}
							lcd_rgb_num(6, current_value);
							break;
					}
					// 保持绿点和高亮显示
				}
				else if(ui3_mode == 2)
				{
					// 从调节模式 → 退出回到选通道模式
					ui3_mode = 1;
					
					// 实时更新LED
					int16_t r, g, b;
					switch(name) {
						case 0: r = red1_zhong; g = green1_zhong; b = blue1_zhong; break;
						case 1: r = red2_zhong; g = green2_zhong; b = blue2_zhong; break;
						case 2: r = red3_zhong; g = green3_zhong; b = blue3_zhong; break;
						case 3: r = red4_zhong; g = green4_zhong; b = blue4_zhong; break;
						default: r = g = b = 0; break;
					}
					WS2812B_SetAllLEDs(name + 1, r * bright * bright_num, g * bright * bright_num, b * bright * bright_num);
					WS2812B_Update(name + 1);
					
					// 保持当前通道高亮，绿点
					lcd_rgb_update(4);
					lcd_rgb_update(5);
					lcd_rgb_update(6);
					lcd_rgb_cai_update(ui3_channel + 4);
					lcd_deng(0, name);  // 绿点
				}
			}
			else if(ui == 2)
			{
				// ✅ 单击切换流水灯模式
				deng_2or3 = !deng_2or3;  // 切换流水灯开关
				printf("UI2 Click: deng_2or3=%d\r\n", deng_2or3);  // 调试输出
				deng_update();
				deng();
				// 刷新指示点显示流水灯状态
				lcd_pei_se_dot(deng_2or3);
			}
			else if(ui == 4)
			{
				// 切换呼吸灯模式
				breath_mode = !breath_mode;
				
				if(breath_mode == 0)
				{
					// 退出呼吸灯模式：清除颜色条，恢复数字显示
					LCD_ClearBreathBar();
					deng();
					LCD_ui4_num_update(bright);
				}
				else
				{
					// 进入呼吸灯模式：清除数字显示区域，准备显示颜色条
					LCD_Fill(20, 85, 160, 85 + 53, BLACK);  // 清除数字区域 (ui4_Y_qi=85, speed_num_high=53)
					breath_phase = 0;
					breath_tick = uwTick;
				}
			}
			else if(ui == 7)  // ✅ 音量界面单击切换调节状态
			{
				volume_state = ~volume_state;
				if(volume_state == 0)
				{
					// 退出调节状态时保存音量到Flash
					deng_update();
				}
			}
		}
		else if(key_state == 2)//双击 - ✅ 返回菜单（原双击开关雾化器功能已移除）
		{
			// 🆕 上报按钮事件到APP
			BLE_ReportButtonEvent("KNOB", "DOUBLE");
			
			// ✅ 菜单界面：双击无操作（已在菜单中）
			if(ui == 5)
			{
				// 菜单界面：双击无反应
			}
			// ✅ 油门模式：双击不返回菜单（保持油门模式）
			else if(wuhuaqi_state == 2)
			{
				// 油门模式：双击无反应（使用旋转或三击退出）
			}
			// ✅ Logo界面：双击返回菜单，记住Logo页面
			else if(ui == 6)
			{
				menu_selected = 5;  // Logo在菜单中是第5项
				ui = 5;
				chu = 5;
			}
			// ✅ 音量界面：双击返回菜单，记住Voice页面
			else if(ui == 7)
			{
				menu_selected = 6;  // Voice在菜单中是第6项
				ui = 5;
				chu = 5;
			}
			// ✅ RGB调色界面：双击返回菜单，保存颜色数据
			else if(ui == 3)
			{
				// 保存颜色到Flash
				deng_update();
				deng();
				// 重置状态机
				ui3_mode = 0;
				ui3_channel = 0;
				menu_selected = 3;
				ui = 5;
				chu = 5;
			}
			// ✅ 亮度界面：双击返回菜单，保存亮度并关闭呼吸灯
			else if(ui == 4)
			{
				// 关闭呼吸灯模式，恢复正常亮度
				breath_mode = 0;
				// ✅ 保存亮度到Flash
				deng_update();
				deng();
				menu_selected = 4;
				ui = 5;
				chu = 5;
			}
			// ✅ 非菜单界面：双击返回菜单
			else if(ui == 2)
			{
				// ✅ 颜色预设界面：保存颜色到Flash并应用
				deng_update();
				deng();
				menu_selected = 2;
				ui = 5;
				chu = 5;
			}
			else
			{
				menu_selected = ui;  // 记住当前 UI
				ui = 5;
				chu = 5;
			}
		}
		else if(key_state == 3)//三击 - 进入油门模式
		{
			if(ui == 1 && chu == 2 && wuhuaqi_state != 2)  // 界面1，非油门模式
			{
				// 🆕 上报按钮事件到APP
				BLE_ReportButtonEvent("KNOB", "TRIPLE");
				
				wuhuaqi_state_saved = wuhuaqi_state;  // 保存进入前的状态
				wuhuaqi_state = 2;
				wuhuaqi_state_old = 2;  // 同步更新
				HAL_GPIO_WritePin(GPIOB,GPIO_PIN_8,GPIO_PIN_SET);
				lcd_wuhuaqi(wuhuaqi_state, speed_value);
				
				// 🏎️ 上报油门模式状态到APP
				BLE_ReportThrottle(1);
				
				// 音频将在按住加速时启动，这里不自动启动
			}
			else if(ui == 1 && ui_old == 0)
			{
				// 保留原有逻辑（如果有其他用途）
				if(speed_value == 0)
				{
					speed_value = 1;
				}
				else
				{
					speed_value = 0;
				}
			}
			if(ui_old == 1)
				ui_old = 0;
		}
		key_state=0;
	}
}
u32 lcd_tick = 0;
int8_t test_num = 0,test_num_old = 0;
void LCD()
{
	if(uwTick - lcd_tick < 50)
		return;
	lcd_tick = uwTick;
	
	// ✅ UI0开机动画已从主循环移除，只在main.c开机时显示一次
	// 主循环界面：UI5(菜单) ↔ UI1(运行模式) ↔ UI2(配色预设) ↔ UI3(RGB调色) ↔ UI4(亮度调节)
	
	// ✅ 菜单界面重构 V2：全屏滑动式菜单 (ui=5)
	if(ui == 5)
	{
		static uint8_t last_menu_selected = 0;
		
		if(chu == 5)
		{
			// 初始化菜单界面：绘制当前选中的页面
			Draw_Menu_Page(menu_selected);
			last_menu_selected = menu_selected;
			chu = 0;
		}
		
		// 页面切换时重绘整个页面
		if(menu_selected != last_menu_selected)
		{
			Draw_Menu_Page(menu_selected);
			last_menu_selected = menu_selected;
		}
		
		// 处理蓝牙自动进入
		if(auto_enter && chu == 0)
		{
			// Logo页面(menu_selected=5)进入ui=6，Voice页面(menu_selected=6)进入ui=7
			if(menu_selected == 5) {
				ui = 6;
				chu = 6;
			} else if(menu_selected == 6) {
				ui = 7;   // ✅ 音量调节界面
				chu = 7;
			} else if(menu_selected <= 4) {
				ui = menu_selected;
				chu = menu_selected;
			}
			auto_enter = 0;
		}
	}
	else if(ui == 1)
	{
		if(chu == 1)
		{
			chu = 2;
			deng();
			LCD_ui1();
			// ✅ 精度修复：使用绝对速度显示
			LCD_picture(10, 2);  // 先显示一个初始值触发刷新
			LCD_picture(current_speed_kmh, 2);
			lcd_wuhuaqi(wuhuaqi_state,speed_value);
		}
		
		// 油门模式的加速减速已经在Encoder()中处理，这里只负责显示更新
		
		// 🔊 音频处理已移至main.c主循环，确保持续调用
		
		// LCD数字刷新逻辑
		static uint32_t last_num_update_tick = 0;
		static int16_t last_displayed_speed = -1;  // 上次显示的速度值
		
		// 油门模式：丝滑刷新，每次速度变化都更新显示
		if(wuhuaqi_state == 2)
		{
			if(current_speed_kmh != last_displayed_speed)
			{
				// 每30ms最多刷新一次，防止SPI拥堵，同时保持丝滑
				if(uwTick - last_num_update_tick >= 30) {
					LCD_picture(current_speed_kmh, 2);
					last_displayed_speed = current_speed_kmh;
					last_num_update_tick = uwTick;
					Num_old = Num;
				}
			}
		}
		// 非油门模式：正常刷新
		else if(Num_old != Num)
		{
			LCD_picture(current_speed_kmh, 2);
			Num_old = Num;
		}
		
		// ✅ 优化：增加定时强制刷新单位，防止单位显示丢失
		static uint32_t last_unit_refresh_tick = 0;
		uint8_t force_unit_refresh = 0;
		
		// 每2秒强制刷新一次单位显示，确保不会丢失
		if(uwTick - last_unit_refresh_tick >= 2000) {
			force_unit_refresh = 1;
			last_unit_refresh_tick = uwTick;
		}
		
		// 在状态变化或强制刷新时更新单位图标
		if(wuhuaqi_state_old != wuhuaqi_state || speed_value_old != speed_value || force_unit_refresh)
		{
			lcd_wuhuaqi(wuhuaqi_state, speed_value);
			wuhuaqi_state_old = wuhuaqi_state;
		}
		Num_old = Num;
		speed_value_old = speed_value;
	}
	else if(ui == 2)
	{
		if(chu == 2)//初始化2界面
		{
			// ✅ 菜单界面重构：重置 UI2 子状态
			pei_se_state = 1;  // ✅ 默认开启，进入即可直接控制
			deng_2or3 = 0;  // ✅ 进入界面时默认关闭流水灯
			deng_update();
			deng();
			chu = 3;
			name_old = 100;
			led_num = 3;
			encoder_delta = 0;  // ✅ 清除编码器增量，防止误触发
			LCD_ui2();
			// ✅ 强制刷新颜色条（清除缓存）
			lcd_pei_se_force(deng_num, 1);
			// ✅ 显示流水灯状态指示点
			printf("UI2 Init: deng_2or3=%d\r\n", deng_2or3);  // 调试输出
			lcd_pei_se_dot(deng_2or3);
		}
        // ✅ 若预设在后台被修改（APP下发），进入界面时强制同步最新预设到LCD
        if(preset_dirty) {
            deng_ui2(); // 按当前 deng_num 应用最新预设到 _zhong 变量
            lcd_pei_se_force(deng_num, pei_se_state); // 强制刷新LCD颜色条
            preset_dirty = 0;
        }
		
		// ✅ 旋转逻辑：如果流水灯开启，旋转时退出流水灯模式
		if(encoder_delta != 0 && deng_2or3 == 1)
		{
			// 旋转时退出流水灯模式
			deng_2or3 = 0;
			deng_update();
			deng();
			lcd_pei_se_dot(deng_2or3);  // 更新指示点为红色
		}
		
		if(pei_se_state)
		{
			int8_t deng_num_before = deng_num;  // 🆕 记录变化前的值
			// ✅ 菜单界面重构：使用统一的 encoder_delta
			deng_num+= encoder_delta;
			if(deng_num<1)
			{
				deng_num = 14;
			}
			else if(deng_num>14)
			{
				deng_num = 1;
			}
			// 🆕 预设变化时上报给APP
			if(deng_num != deng_num_before)
			{
				BLE_ReportPreset(deng_num);
			}
			
			// ✅ 流水灯模式下跳过普通的LCD和LED更新，由Streamlight_Process统一处理
			if(deng_2or3 == 0) {
				lcd_pei_se(deng_num,pei_se_state);
				deng_ui2();
			}
			lcd_pei_se_dot(deng_2or3);  // ✅ 指示点始终更新
		}
		else
		{
			lcd_pei_se(0,pei_se_state);
			// ✅ 菜单界面重构：使用统一的 encoder_delta
			brt_nothing+= encoder_delta;
		}
		pei_se_state_old = pei_se_state;
	}
	else if(ui == 3)
	{
		if(chu == 3)//初始化3界面,调色
		{
			// ═══════════════════════════════════════════════════════════════════
			// 🎨 UI3 RGB调色界面初始化（重构优化）
			// ═══════════════════════════════════════════════════════════════════
			
			// ✅ 进入RGB调色界面时关闭流水灯（调色优先级高于流水灯）
			if(deng_2or3 == 1) {
				deng_2or3 = 0;
				deng_update();
			}
			
			chu = 4;
			deng();
			LCD_ui3();
			
			// 重置状态机
			ui3_mode = 0;      // 默认选灯带模式
			ui3_channel = 0;   // R通道
			name = 0;          // 默认选中Middle灯带
			name_old = 0;
			
			// 显示初始状态
			lcd_name_update(name);
			lcd_rgb_update(4);
			lcd_rgb_update(5);
			lcd_rgb_update(6);
			lcd_deng(1, name);  // 1=红点(选灯带模式)
			
			// 清除编码器累积值
			encoder_delta = 0;
		}
		
		// 调用统一的调色处理函数
		tiao_se();
	}
	else if(ui == 4)
	{
		if(chu == 4)
		{
			// ═══════════════════════════════════════════════════════════════════
			// 🌟 UI4 亮度调节界面初始化
			// ═══════════════════════════════════════════════════════════════════
			
			// ✅ 进入亮度界面时关闭流水灯（呼吸灯优先级高于流水灯）
			if(deng_2or3 == 1) {
				deng_2or3 = 0;
				deng_update();
			}
			
			breath_mode = 0;  // 默认正常模式(红点)
			breath_phase = 0;
			chu = 5;
			deng();
			LCD_ui4();
			LCD_ui4_num_update(bright);
			lcd_ui4_deng(0);  // 0=红点(正常模式)
		}
		
		// 调用亮度/呼吸灯处理函数
		deng_ui4();
		
		// 更新指示灯显示
		lcd_ui4_deng(breath_mode);  // 0=红点, 1=绿点
	}
	else if(ui == 6)  // Logo界面 - 支持多槽位切换
	{
		// 多槽位管理变量
		static uint8_t logo_view_slot = 0;      // 当前查看的槽位
		static uint8_t logo_slot_count = 0;     // 有效槽位数量
		static uint8_t logo_ui_initialized = 0; // UI初始化标志
		static uint32_t last_slot_switch_tick = 0;  // 防抖时间戳
		static uint32_t logo_key_press_tick = 0;    // 长按计时起点
		static uint8_t logo_long_press_triggered = 0; // 长按已触发标志
		
		if(chu == 6)
		{
			chu = 0;
			logo_ui_initialized = 0;
			logo_long_press_triggered = 0;  // 重置长按标志
			Logo_ClearErrorFlag();  // 清除错误标志
			
			// 初始化：统计有效槽位，获取激活槽位
			logo_slot_count = Logo_CountValidSlots();
			logo_view_slot = Logo_GetActiveSlot();
			
			// 如果激活槽位无效，找第一个有效槽位
			if (!Logo_IsSlotValid(logo_view_slot)) {
				for (uint8_t i = 0; i < 3; i++) {
					if (Logo_IsSlotValid(i)) {
						logo_view_slot = i;
						break;
					}
				}
			}
			
			// 清屏并显示Logo（纯净显示，无任何文字）
			LCD_Fill(0, 0, 240, 240, BLACK);
			
			if (logo_slot_count > 0) {
				Logo_ShowSlot(logo_view_slot);
			} else {
				// 无有效Logo，显示提示文字（不显示开机动画）
				LCD_ShowString(60, 100, "No Custom Logo", WHITE, BLACK, 16, 0);
				LCD_ShowString(45, 130, "Upload from APP", GRAY, BLACK, 16, 0);
			}
			
			logo_ui_initialized = 1;
		}
		
		// 旋转切换槽位 (只在有多个有效槽位时)
		if (logo_ui_initialized && logo_slot_count > 1 && encoder_delta != 0) {
			// 防抖：至少间隔200ms
			if (uwTick - last_slot_switch_tick >= 200) {
				uint8_t old_slot = logo_view_slot;
				
				if (encoder_delta > 0) {
					logo_view_slot = Logo_NextValidSlot(logo_view_slot);
				} else {
					logo_view_slot = Logo_PrevValidSlot(logo_view_slot);
				}
				
				if (logo_view_slot != old_slot) {
					// 清屏并显示新槽位Logo（纯净显示）
					LCD_Fill(0, 0, 240, 240, BLACK);
					Logo_ShowSlot(logo_view_slot);
				}
				
				last_slot_switch_tick = uwTick;
			}
		}
		
		// ═══════════════════════════════════════════════════════════════════
		// 🔥 长按删除Logo功能（≥2秒）- 圆角颜色条进度条
		// ═══════════════════════════════════════════════════════════════════
		// 读取当前按键状态 (低电平有效，按下=0)
		uint8_t key_pressed = (HAL_GPIO_ReadPin(ENC_PORT, ENC_KEY_PIN) == GPIO_PIN_RESET) ? 1 : 0;
		static uint8_t key_pressed_old = 0;
		static uint8_t delete_progress_shown = 0;  // 进度条是否已显示
		static uint32_t last_progress_update = 0;  // 上次进度更新时间
		static uint16_t last_width = 0;            // 上次绘制的宽度
		
		// 进度条参数（屏幕底部居中，圆角条样式）
		// LCD_DrawRoundedBar参数：中心x, 中心y, 总宽度, 高度, 颜色
		#define PBAR_CX     120      // 中心X
		#define PBAR_CY     215      // 中心Y
		#define PBAR_W      160      // 总宽度
		#define PBAR_H      12       // 高度
		
		// 检测按键按下边沿，记录按下时间
		if (logo_ui_initialized && key_pressed == 1 && key_pressed_old == 0) {
			logo_key_press_tick = uwTick;
			logo_long_press_triggered = 0;
			delete_progress_shown = 0;
			last_width = 0;
		}
		
		// 长按进度显示（按住超过300ms开始显示进度）
		if (logo_ui_initialized && logo_slot_count > 0 && key_pressed == 1 && 
		    !logo_long_press_triggered) {
			uint32_t hold_time = uwTick - logo_key_press_tick;
			
			// 超过300ms开始显示删除进度
			if (hold_time >= 300 && hold_time < 2000) {
				// 每40ms更新一次
				if (uwTick - last_progress_update >= 40) {
					// 计算当前宽度 (300ms~2000ms 映射到 PBAR_H ~ PBAR_W)
					// 最小宽度为PBAR_H（保证圆角效果）
					uint16_t current_width = PBAR_H + (hold_time - 300) * (PBAR_W - PBAR_H) / 1700;
					if (current_width > PBAR_W) current_width = PBAR_W;
					
					if (!delete_progress_shown) {
						// 无背景条，直接在黑色背景上显示白色进度
						delete_progress_shown = 1;
					}
					
					// 绘制白色进度条（圆角，从左边开始）
					// 计算进度条中心位置（左对齐）
					uint16_t progress_cx = PBAR_CX - PBAR_W/2 + current_width/2;
					LCD_DrawRoundedBar(progress_cx, PBAR_CY, current_width, PBAR_H, WHITE);
					
					last_width = current_width;
					last_progress_update = uwTick;
				}
			}
			
			// 长按达到2秒，执行删除
			if (hold_time >= 2000) {
				// 进度条变绿色表示完成
				LCD_DrawRoundedBar(PBAR_CX, PBAR_CY, PBAR_W, PBAR_H, 0x07E0);  // 绿色
				
				// 执行删除
				Logo_DeleteSlot(logo_view_slot);
				logo_slot_count = Logo_CountValidSlots();
				
				// 延迟显示提示
				HAL_Delay(300);
				
				// 刷新显示
				LCD_Fill(0, 0, 240, 240, BLACK);
				if (logo_slot_count > 0) {
					logo_view_slot = Logo_NextValidSlot(logo_view_slot);
					if (!Logo_IsSlotValid(logo_view_slot)) {
						for (uint8_t i = 0; i < 3; i++) {
							if (Logo_IsSlotValid(i)) {
								logo_view_slot = i;
								break;
							}
						}
					}
					Logo_ShowSlot(logo_view_slot);
				} else {
					Logo_ShowBoot();
				}
				
				logo_long_press_triggered = 1;
				delete_progress_shown = 0;
				key_state = 0xFF;
			}
		}
		
		// 按键释放时清除进度条（如果未完成删除）
		if (key_pressed == 0 && key_pressed_old == 1) {
			if (delete_progress_shown && !logo_long_press_triggered) {
				// 取消删除，恢复Logo显示
				LCD_Fill(0, 0, 240, 240, BLACK);
				if (logo_slot_count > 0) {
					Logo_ShowSlot(logo_view_slot);
				} else {
					Logo_ShowBoot();
				}
			}
			logo_long_press_triggered = 0;
			delete_progress_shown = 0;
		}
		
		// 短按无功能（开机动画固定为默认，不可更改）
		
		key_pressed_old = key_pressed;
	}
	else if(ui == 7)  // ✅ 音量调节界面 - 简洁大数字风格
	{
		if(chu == 7)
		{
			// 初始化音量界面（纯黑背景 + 大数字）
			chu = 0;
			LCD_ui6();  // 纯黑背景
			LCD_ui6_num_update(volume);  // 显示当前音量大数字
			lcd_ui6_deng(0);  // 显示红点指示灯
		}
		
		// 直接调节音量，无需激活状态
		volume_ui6();
	}
	// ✅ 主循环只包含UI1-4，UI0只在开机时显示
}




