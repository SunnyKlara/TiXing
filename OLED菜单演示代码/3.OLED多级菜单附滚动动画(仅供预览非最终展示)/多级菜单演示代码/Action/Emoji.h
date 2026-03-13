/* ---------------- Emoji.h ---------------- */

#ifndef __Emoji_H
#define __Emoji_H


// 眼睛结构
typedef struct {
    int16_t x, y;           // 中心坐标
    uint8_t width, height;  // 椭圆宽高（半轴）
    uint8_t open_ratio;     // 睁开程度 0-100%
    int8_t  pupil_offset_x; // 瞳孔X偏移（视线方向）
    int8_t  pupil_offset_y; // 瞳孔Y偏移
} Eye_t;

// 表情状态机
typedef enum {
    FACE_NEUTRAL,   // 平静
    FACE_BLINK,     // 眨眼
    FACE_HAPPY,     // 开心
    FACE_SAD,       // 悲伤
    FACE_ANGRY,     // 生气
    FACE_SURPRISED, // 惊讶
    FACE_SLEEPY     // 困倦
} FaceState_t;

// 表情管理器
typedef struct {
    Eye_t left_eye;
    Eye_t right_eye;
    FaceState_t state;
    FaceState_t next_state;
    uint32_t anim_tick;     // 动画时钟
    uint8_t  is_blinking;   // 是否正在眨眼
    int8_t   face_offset_x; // 整体偏移（摇头效果）
    int8_t   face_offset_y;
	int8_t target_pupil_x;  // 目标视线X
    int8_t target_pupil_y;  // 目标视线Y
    float pupil_vel_x;      // 视线速度X（如果.h不能用float，可改为int8_t，在.c中用静态变量）
    float pupil_vel_y;      // 视线速度Y
} FaceManager_t;	

void Emoji_KeyAction(uint8_t Event);
void Emoji_Enter(void);
void Emoji_Display(void);	//
void Emoji_Exit(void);
void Show_Emoji(FaceManager_t *face);
void Emoji_Init(FaceManager_t *face);

// API接口
//void Face_Init(FaceManager_t *face);
void Face_Update(FaceManager_t *face);  // 主循环调用，非阻塞
void Face_SetState(FaceManager_t *face, FaceState_t state);
void Face_LookAt(FaceManager_t *face, int8_t x, int8_t y); // 看向某点
void Face_Blink(FaceManager_t *face);   // 触发眨眼
//void Face_Draw(FaceManager_t *face);    // 绘制到缓冲区
#endif