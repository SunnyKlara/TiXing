/* ---------------- Emoji.c ---------------- */
/* ---------------- 该应用并不成熟 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include <stdlib.h>  // 新增：用于rand()
#include "OLED.h"
#include "Animation.h"
#include "MyTimer.h"
#include "Emoji.h"
#include "Key.h"
#include "MyRTC.h"
#include "Emoji_Image.h"
#include "Delay.h"
#include "Action.h"
#include <math.h>

/*
编码器逆时针转:EVENT 1
编码器顺时针转:EVENT 2
按键中断1:EVENT 3
按键中断2:EVENT 4
*/
/* 按键事件值 */
#define ENC_CCW     2
#define ENC_CW      1
#define KEY_OK      4
#define KEY_BACK    3

// 动画参数
#define BLINK_SPEED     4       // 眨眼速度
#define LOOK_SPEED      1       // 视线移动速度（保留原值，内部使用物理惯性替代）
#define BREATH_SPEED    1       // 呼吸效果速度

typedef enum {
    Emoji_READY,
} Emoji_STATE;

/*---------- 全局变量 ----------*/
static Emoji_STATE state = Emoji_READY;

FaceManager_t myFace;
static uint32_t tick = 0;  // 移到静态全局，避免栈溢出

// 视线物理系统静态变量（替代结构体中可能不存在的字段，如果你已在.h中添加这些字段，可删除）
static float pupil_vel_x = 0;
static float pupil_vel_y = 0;
static int8_t target_pupil_x = 0;
static int8_t target_pupil_y = 0;
static uint16_t next_saccade = 100;  // 下次微动时间

// 困倦状态专用变量
static uint8_t sleepy_eye_state = 0;  // 0:正常, 1:左眼闭, 2:右眼闭, 3:双眼半闭
static uint16_t sleepy_timer = 0;

// ===== 新增：眼睛目标值（用于状态间平滑过渡）=====
static uint8_t target_left_open = 100;
static uint8_t target_right_open = 100;
static uint8_t target_left_width = 12;
static uint8_t target_right_width = 12;

/*========== 对外接口 ==========*/
void Emoji_Enter(void)
{
    state = Emoji_READY;
	ActionDisplay_Init(Emoji_Display);
	ActionKey_Init(Emoji_KeyAction);
	OLED_ClearBuf();
	Timer_SetPrescaler(10*menuOptions.SystemClock/72);

	Emoji_Init(&myFace);
    TaskTimer.UpdateTask = 1;
    
    // 初始化随机种子（如果有RTC或ADC可用更随机值）
    srand(myFace.anim_tick);
}

void Emoji_KeyAction(uint8_t event)
{
    switch (state) 
	{
		case Emoji_READY:
			if (event == KEY_OK) {  }
			else if (event == KEY_BACK) { Emoji_Exit(); }
			break;
    }
}

void Emoji_Display(void)
{
    
	OLED_ClearBuf();               // 双缓冲清 0
    switch (state) {
    case Emoji_READY:
        Show_Emoji(&myFace);
        break;

    }
}



void Emoji_Init(FaceManager_t *face)
{
	// 初始化眼睛位置（128x64屏幕）
    face->left_eye.x = 40;
    face->left_eye.y = 32;
    face->left_eye.width = 12;
    face->left_eye.height = 14;
    face->left_eye.open_ratio = 100;
    face->left_eye.pupil_offset_x = 0;
    face->left_eye.pupil_offset_y = 0;
    
    face->right_eye.x = 88;
    face->right_eye.y = 32;
    face->right_eye.width = 12;
    face->right_eye.height = 14;
    face->right_eye.open_ratio = 100;
    face->right_eye.pupil_offset_x = 0;
    face->right_eye.pupil_offset_y = 0;
    
    face->state = FACE_NEUTRAL;
    face->anim_tick = 0;
    face->is_blinking = 0;
    face->face_offset_x = 0;
    face->face_offset_y = 0;
	
    // 重置物理系统
    pupil_vel_x = 0;
    pupil_vel_y = 0;
    target_pupil_x = 0;
    target_pupil_y = 0;
    next_saccade = 100;
    tick = 0;
    
    // 重置困倦状态（新增）
    sleepy_eye_state = 0;
    sleepy_timer = 0;
    
    // 重置目标值
    target_left_open = 100;
    target_right_open = 100;
    target_left_width = 12;
    target_right_width = 12;
}


// 绘制单个眼睛（利用你现有的画椭圆函数）
void Draw_Eye(Eye_t *eye, int8_t offset_x, int8_t offset_y) {
    int16_t real_x = eye->x + offset_x;
    int16_t real_y = eye->y + offset_y;
    
    // 根据睁开程度调整高度
    uint8_t h = (eye->height * eye->open_ratio) / 100;
    if (h < 2) h = 2;  // 最小高度
    
    // 画眼白（空心椭圆）
    OLED_DrawEllipse(real_x, real_y, eye->width, h, 0);
    
    // 画瞳孔（实心小椭圆，随视线偏移）
    if (eye->open_ratio > 20) {  // 睁得够大才画瞳孔（阈值降低）
        int16_t pupil_x = real_x + eye->pupil_offset_x;
        int16_t pupil_y = real_y + eye->pupil_offset_y;
        
        // 瞳孔大小随睁眼程度微调（惊讶时变大）
        uint8_t pupil_r = (eye->open_ratio > 110) ? 5 : 4;
        OLED_DrawEllipse(pupil_x, pupil_y, pupil_r, pupil_r, 1);  // 实心
        
        // ===== 高光（灵魂！）=====
        // 左上高光点，让眼睛有反光感
        OLED_DrawPixel(pupil_y - 2, pupil_x - 1);
        OLED_DrawPixel(pupil_y - 1, pupil_x - 2);
    }
    
    // ===== 眼睑阴影（闭眼时的弧线）=====
    if (eye->open_ratio < 90) {
        // 在眼睛上方画弧线表示上眼睑
        uint8_t eyelid_h = (90 - eye->open_ratio) / 6;  // 闭眼越厉害，阴影越明显
        int8_t i;
        for(i = -eye->width + 2; i <= eye->width - 2; i++) {
            int8_t arc_y = real_y - h + 1 + (i*i)/(eye->width*3);
            if (arc_y < real_y && arc_y > real_y - h - eyelid_h) {
                OLED_DrawPixel(arc_y, real_x + i);
            }
        }
    }
}

// 二次贝塞尔曲线画嘴（更平滑）
static void Draw_Bezier(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t dense) {
    float t;
    for(t = 0; t <= 1; t += 0.05f/dense) {
        int16_t x = (1-t)*(1-t)*x0 + 2*(1-t)*t*x1 + t*t*x2;
        int16_t y = (1-t)*(1-t)*y0 + 2*(1-t)*t*y1 + t*t*y2;
        OLED_DrawPixel(y, x);
    }
}

// 绘制嘴巴（根据状态变化）
void Draw_Mouth(FaceManager_t *face, int8_t offset_x, int8_t offset_y) 
{
    int16_t mx = 64 + offset_x;
    int16_t my = 50 + offset_y;
    int8_t i;
    
    switch(face->state) {
        case FACE_HAPPY:
            // 笑脸（倒过来的椭圆下半部分，用DrawPixel画弧线）
            for(i = -10; i <= 10; i++) {
                int8_t y = (i*i)/20;  // 抛物线模拟
                OLED_DrawPixel(my + y, mx + i);
            }
            break;
            
        case FACE_SAD:
            // 悲伤嘴：倒U型，向下弯曲明显（比原来更明显的悲伤）
            Draw_Bezier(mx-10, my+3, mx, my-5, mx+10, my+3, 1);
            // 加粗嘴角向下
            OLED_DrawPixel(my+3, mx-9);
            OLED_DrawPixel(my+3, mx+9);
            OLED_DrawPixel(my+2, mx-8);
            OLED_DrawPixel(my+2, mx+8);
            break;
            
        case FACE_SURPRISED:
            // O型嘴（小圆）
            OLED_DrawEllipse(mx, my, 4, 5, 1);
            break;
            
        case FACE_ANGRY:
            // 倒八字眉+直线嘴
            OLED_DrawPixel(my, mx-8);
            OLED_DrawPixel(my, mx+8);
            for(i = -8; i <= 8; i++) {
                OLED_DrawPixel(my+2, mx+i);
            }
            break;
            
        case FACE_SLEEPY:
            // 困倦嘴：O型打哈欠或者扁嘴
            if (sleepy_eye_state == 3) {
                // 打哈欠（大O型）
                OLED_DrawEllipse(mx, my+2, 5, 6, 1);
            } else {
                // 无精打彩的小嘴
                for(i = -4; i <= 4; i++) {
                    OLED_DrawPixel(my+1, mx+i);
                }
            }
            break;
            
        default:
            // 默认小 smile
            OLED_DrawPixel(my, mx-4);
            OLED_DrawPixel(my, mx+4);
            OLED_DrawPixel(my+1, mx-3);
            OLED_DrawPixel(my+1, mx+3);
            break;
    }
}




void Show_Emoji(FaceManager_t *face)
{
	tick++;
	
    // 示例：每隔一段时间改变情绪（扩展到6种状态）
    if(tick % 800 == 0) {
        static uint8_t mood = 0;
        mood = (mood + 1) % 6;  // 增加到6种状态循环
        
        switch(mood) {
            case 0: 
                Face_SetState(face, FACE_NEUTRAL); 
                Face_LookAt(face, 0, 0);
                break;
            case 1: 
                Face_SetState(face, FACE_HAPPY); 
                Face_LookAt(face, -3, -2);  // 开心时害羞看左下
                break;
            case 2: 
                Face_SetState(face, FACE_SURPRISED); 
                Face_LookAt(face, 0, 0);    // 惊讶时正视
                break;
            case 3: 
                Face_SetState(face, FACE_ANGRY); 
                Face_LookAt(face, 6, 1);    // 生气时瞪向一侧
                break;
            case 4: 
                Face_SetState(face, FACE_SAD); 
                Face_LookAt(face, 0, 3);    // 悲伤时视线向下
                break;
            case 5: 
                Face_SetState(face, FACE_SLEEPY); 
                Face_LookAt(face, -2, 2);   // 困倦时视线慵懒
                break;
        }
    }
    
    // 视线自动漫游（避开情绪切换瞬间，困倦和悲伤时减少漫游）
    if(tick % 600 == 0 && (tick % 800) > 100 && face->state != FACE_SLEEPY && face->state != FACE_SAD) {
        int8_t look_x = (rand() % 12) - 6;  // -6 到 6
        int8_t look_y = (rand() % 8) - 4;   // -4 到 4
        Face_LookAt(face, look_x, look_y);
    }
	
	
	Face_Update(face);
	
    // 清屏
    OLED_ClearBuf();
    
    // 计算带3D呼吸的偏移
    int8_t offset_x = face->face_offset_x;
    int8_t offset_y = face->face_offset_y;
    
    // 绘制双眼（带高光）
    Draw_Eye(&face->left_eye, offset_x, offset_y);
    Draw_Eye(&face->right_eye, offset_x, offset_y);
    
    // 绘制嘴巴
    Draw_Mouth(face, offset_x, offset_y);
    
    // 绘制眉毛（根据状态）
    if(face->state == FACE_ANGRY) 
	{
        // 愤怒眉：倒八字，随呼吸微动
        int8_t brow_y = 20 + offset_y;
        int8_t i;
        for(i=0; i<10; i++) {
            OLED_DrawPixel(brow_y - i/2, 32 + offset_x + i);   // 左眉（下降）
            OLED_DrawPixel(brow_y - i/2, 96 + offset_x - i);   // 右眉（下降）
        }
    } else if(face->state == FACE_HAPPY) {
        // 开心眉：弯弯的（使用小弧线模拟）
        Draw_Bezier(30+offset_x, 18+offset_y, 38+offset_x, 14+offset_y, 46+offset_x, 18+offset_y, 1);
        Draw_Bezier(82+offset_x, 18+offset_y, 90+offset_x, 14+offset_y, 98+offset_x, 18+offset_y, 1);
    } else if(face->state == FACE_SAD) {
        // 悲伤八字眉（向内向下倾斜更明显）
        int8_t brow_y = 16 + offset_y;
        int8_t i;
        for(i=0; i<10; i++) {
            OLED_DrawPixel(brow_y + i/2, 30 + offset_x + i);  // 左眉下降
            OLED_DrawPixel(brow_y + i/2, 98 + offset_x - i);  // 右眉下降
        }
        // 加粗眉毛
        for(i=0; i<8; i++) {
            OLED_DrawPixel(brow_y + 1 + i/2, 32 + offset_x + i);
            OLED_DrawPixel(brow_y + 1 + i/2, 96 + offset_x - i);
        }
    } else if(face->state == FACE_SLEEPY) {
        // 困倦平眉，稍微下垂，无精打彩
        int8_t i;
        for(i=0; i<12; i++) {
            OLED_DrawPixel(19 + offset_y, 30 + offset_x + i);
            OLED_DrawPixel(19 + offset_y, 86 + offset_x + i);
        }
    }
}


// 非阻塞动画更新（在主循环中调用）
void Face_Update(FaceManager_t *face) {
    face->anim_tick++;
    
    // ===== 1. 状态间平滑过渡（渐变到目标开合度）=====
    // 如果不是在眨眼，就进行状态间的平滑过渡
    if(!face->is_blinking) {
        uint8_t speed = 4;  // 过渡速度，每帧变化4%
        
        // 左眼开合度过渡
        if(face->left_eye.open_ratio < target_left_open) {
            face->left_eye.open_ratio += speed;
            if(face->left_eye.open_ratio > target_left_open) 
                face->left_eye.open_ratio = target_left_open;
        } else if(face->left_eye.open_ratio > target_left_open) {
            face->left_eye.open_ratio -= speed;
            if(face->left_eye.open_ratio < target_left_open)
                face->left_eye.open_ratio = target_left_open;
        }
        
        // 右眼开合度过渡
        if(face->right_eye.open_ratio < target_right_open) {
            face->right_eye.open_ratio += speed;
            if(face->right_eye.open_ratio > target_right_open)
                face->right_eye.open_ratio = target_right_open;
        } else if(face->right_eye.open_ratio > target_right_open) {
            face->right_eye.open_ratio -= speed;
            if(face->right_eye.open_ratio < target_right_open)
                face->right_eye.open_ratio = target_right_open;
        }
        
        // 眼睛宽度过渡（用于挤压感和困倦状态的随机单眼半闭）
        if(face->left_eye.width < target_left_width) {
            face->left_eye.width++;
        } else if(face->left_eye.width > target_left_width) {
            face->left_eye.width--;
        }
        
        if(face->right_eye.width < target_right_width) {
            face->right_eye.width++;
        } else if(face->right_eye.width > target_right_width) {
            face->right_eye.width--;
        }
    }
    
    // ===== 2. 眨眼动画处理（带挤压感）=====
    if(face->is_blinking) {
        static uint8_t blink_phase = 0;
        blink_phase++;
        
        if(blink_phase <= 10) {
            // 闭眼：正弦前半段（快闭）
            float t = (blink_phase / 10.0f) * 3.14159f / 2;
            uint8_t ratio = (uint8_t)(100 * cos(t));
            face->left_eye.open_ratio = ratio;
            face->right_eye.open_ratio = ratio;
            
            // 挤压效果：闭眼时眼睛变窄
            uint8_t squeeze = (100 - ratio) / 8;
            face->left_eye.width = 12 - squeeze;
            face->right_eye.width = 12 - squeeze;
        } else if(blink_phase <= 20) {
            // 睁眼：正弦后半段（慢睁）
            float t = ((blink_phase-10) / 10.0f) * 3.14159f / 2;
            uint8_t ratio = (uint8_t)(100 * sin(t));
            face->left_eye.open_ratio = ratio;
            face->right_eye.open_ratio = ratio;
            
            // 恢复宽度
            uint8_t squeeze = (100 - ratio) / 8;
            face->left_eye.width = 12 - squeeze;
            face->right_eye.width = 12 - squeeze;
        } else {
            // 结束眨眼，回到目标状态（通过上面的平滑过渡自动完成）
            face->is_blinking = 0;
            blink_phase = 0;
            // 不直接赋值，让上面的过渡系统自动回到 target_open
        }
    } else if(face->state != FACE_SLEEPY) {  // 非困倦状态正常眨眼
        // 随机眨眼（每200-500个tick随机眨眼一次）
        if((face->anim_tick % 250) == 0) {
            if(rand() % 100 < 35) {  // 35%概率
                Face_Blink(face);
            }
        }
    }
    
    // ===== 3. 困倦状态特殊处理：随机单眼半闭（使用目标值过渡）=====
    if(face->state == FACE_SLEEPY) {
        sleepy_timer++;
        
        // 每100-300tick随机改变困倦状态
        if(sleepy_timer > 100 + (rand() % 200)) {
            sleepy_timer = 0;
            uint8_t r = rand() % 100;
            if(r < 25) {
                sleepy_eye_state = 0;  // 双眼睁开（努力清醒）
                target_left_open = 100;   // 设置目标值，让平滑过渡处理
                target_right_open = 100;
                target_left_width = 12;
                target_right_width = 12;
            } else if(r < 50) {
                sleepy_eye_state = 1;     // 左眼半闭（打瞌睡）
                target_left_open = 35;
                target_right_open = 75;
                target_left_width = 10;
                target_right_width = 12;
            } else if(r < 75) {
                sleepy_eye_state = 2;     // 右眼半闭
                target_left_open = 75;
                target_right_open = 35;
                target_left_width = 12;
                target_right_width = 10;
            } else {
                sleepy_eye_state = 3;     // 双眼半闭（快睡着了）
                target_left_open = 30;
                target_right_open = 30;
                target_left_width = 11;
                target_right_width = 11;
            }
        }
        
        // 困倦时偶尔整体眨眼（重置为双眼同步）
        if((face->anim_tick % 400) == 0 && (rand() % 100 < 20)) {
            Face_Blink(face);
        }
    }
    
    // ===== 4. 视线物理惯性（弹簧阻尼系统）=====
    float k = 0.12f;   // 弹簧系数（软硬适中）
    float d = 0.75f;   // 阻尼系数（惯性大小）
    
    // 计算指向目标的加速度
    float ax = (target_pupil_x - face->left_eye.pupil_offset_x) * k;
    float ay = (target_pupil_y - face->left_eye.pupil_offset_y) * k;
    
    // 更新速度
    pupil_vel_x += ax;
    pupil_vel_y += ay;
    pupil_vel_x *= d;  // 阻尼衰减
    pupil_vel_y *= d;
    
    // 更新位置（带最小速度限制，防止微振）
    if(fabs(pupil_vel_x) > 0.1f || fabs(pupil_vel_x - ax) > 0.5f) {
        face->left_eye.pupil_offset_x += (int8_t)pupil_vel_x;
    }
    if(fabs(pupil_vel_y) > 0.1f || fabs(pupil_vel_y - ay) > 0.5f) {
        face->left_eye.pupil_offset_y += (int8_t)pupil_vel_y;
    }
    
    // 限制范围
    if(face->left_eye.pupil_offset_x > 8) face->left_eye.pupil_offset_x = 8;
    if(face->left_eye.pupil_offset_x < -8) face->left_eye.pupil_offset_x = -8;
    if(face->left_eye.pupil_offset_y > 6) face->left_eye.pupil_offset_y = 6;
    if(face->left_eye.pupil_offset_y < -6) face->left_eye.pupil_offset_y = -6;
    
    face->right_eye.pupil_offset_x = face->left_eye.pupil_offset_x;
    face->right_eye.pupil_offset_y = face->left_eye.pupil_offset_y;
    
    // ===== 5. 微动（Saccade：生物眼球震颤）=====
    if(face->anim_tick > next_saccade && face->state != FACE_SLEEPY && face->state != FACE_SAD) {
        // 在当前目标附近添加随机扰动
        int8_t noise_x = (rand() % 5) - 2;  // -2到2
        int8_t noise_y = (rand() % 3) - 1;  // -1到1
        target_pupil_x += noise_x;
        target_pupil_y += noise_y;
        
        // 限制目标范围
        if(target_pupil_x > 7) target_pupil_x = 7;
        if(target_pupil_x < -7) target_pupil_x = -7;
        if(target_pupil_y > 5) target_pupil_y = 5;
        if(target_pupil_y < -5) target_pupil_y = -5;
        
        next_saccade = face->anim_tick + 80 + (rand() % 150);  // 80-230tick后下次微动
    }
    
    // ===== 6. 3D呼吸效果（叠加噪声）=====
    float breath = sin(face->anim_tick * 0.08f) * 1.5f;      // 慢速呼吸
    float noise = sin(face->anim_tick * 0.25f) * 0.3f;       // 高频抖动
    float drift = cos(face->anim_tick * 0.03f) * 0.5f;       // 长期漂移
    
    // 困倦时头部更低（向下偏移），悲伤时轻微低头
    if(face->state == FACE_SLEEPY) {
        face->face_offset_y = (int8_t)(breath + noise + 3);  // 整体向下3像素（低头）
        face->face_offset_x = (int8_t)(drift * 0.5f);        // 困倦时摇晃减少
    } else if(face->state == FACE_SAD) {
        face->face_offset_y = (int8_t)(breath + noise + 1);  // 轻微低头
        face->face_offset_x = (int8_t)(drift * 0.3f);        // 悲伤时几乎不摇
    } else {
        face->face_offset_y = (int8_t)(breath + noise);
        face->face_offset_x = (int8_t)(drift);
    }
}

void Face_SetState(FaceManager_t *face, FaceState_t new_state) {
    face->state = new_state;
    
    // 设置目标值（不是直接赋值），让平滑过渡系统处理
    switch(new_state) {
        case FACE_SLEEPY:
            target_left_open = 40;    // 双眼半闭目标
            target_right_open = 40;
            target_left_width = 12;
            target_right_width = 12;
            sleepy_eye_state = 3;
            sleepy_timer = 0;
            break;
            
        case FACE_SAD:
            target_left_open = 85;    // 稍微眯眼
            target_right_open = 85;
            target_left_width = 12;
            target_right_width = 12;
            break;
            
        case FACE_SURPRISED:
            // 删除直接赋值，让过渡系统自动从当前值渐变到125
            target_left_open = 125;   // 睁大目标
            target_right_open = 125;
            target_left_width = 12;
            target_right_width = 12;
            break;
            
        case FACE_ANGRY:
            target_left_open = 100;
            target_right_open = 100;
            target_left_width = 12;
            target_right_width = 12;
            break;
            
        case FACE_HAPPY:
            target_left_open = 100;
            target_right_open = 100;
            target_left_width = 12;
            target_right_width = 12;
            break;
            
        default: // FACE_NEUTRAL
            target_left_open = 100;
            target_right_open = 100;
            target_left_width = 12;
            target_right_width = 12;
            break;
    }
}

void Face_LookAt(FaceManager_t *face, int8_t x, int8_t y) {
    // x,y范围 -10 到 10，表示看向的方向
    // 限制范围
    if(x > 8) x = 8; if(x < -8) x = -8;
    if(y > 6) y = 6; if(y < -6) y = -6;
    
    // 设置目标，实际移动由物理系统处理
    target_pupil_x = x;
    target_pupil_y = y;
}

void Face_Blink(FaceManager_t *face) 
{
    if(!face->is_blinking) {
        face->is_blinking = 1;
    }
}


void Emoji_Exit(void)
{
	Timer_SetPrescaler(720);
	ACTION_Exit();
	
}