/* ---------------- MPU6050.c ---------------- */
/* ---------------- 3D立方体绘制 - 透视虚实线效果 ---------------- */
#include "stm32f10x.h"
#include "Image.h"
#include <string.h>
#include <math.h>
#include "OLED.h"
#include "Animation.h"
#include "MyTimer.h"
#include "MPU6050.h"
#include "Key.h"
#include "Delay.h"
#include "SelectorPhysics.h"
#include "Action.h"

/* 事件定义 */
#define ENC_CW      1
#define ENC_CCW     2
#define KEY_BACK    3
#define KEY_OK      4

/* 应用状态 */
typedef enum {
    MPU6050_READY,
    MPU6050_PLAYING,
    MPU6050_PAUSE,
    MPU6050_OVER
} MPU6050_STATE;

/*---------- 全局变量 ----------*/
static MPU6050_STATE state = MPU6050_READY;
static SelectorPhysics menu_selector;
static uint8_t selected_index = 0;

/*---------- 3D立方体参数 ----------*/
#define CUBE_SIZE   18
#define CENTER_X    96      // 建议居中显示
#define CENTER_Y    32
#define DASH_STEP   2       // 虚线步长（4像素实线+4像素空白）

/* 3D顶点结构 */
typedef struct {
    float x, y, z;
} Vertex3D;

/* 2D投影点结构 */
typedef struct {
    int16_t x, y;
} Vertex2D;

/* 立方体8个顶点（单位立方体，中心在原点） */
static const Vertex3D cube_vertices[8] = {
    {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}
};

/* 立方体12条边（顶点索引） */
static const uint8_t cube_edges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0},  // 后面四边
    {4,5}, {5,6}, {6,7}, {7,4},  // 前面四边
    {0,4}, {1,5}, {2,6}, {3,7}   // 连接前后的四条边
};

/* 旋转角度 */
static float angle_x = 0.0f;
static float angle_y = 0.0f;
static float angle_z = 0.0f;

/*---------- 静态函数声明 ----------*/
static void RotateX(Vertex3D *v, float angle);
static void RotateY(Vertex3D *v, float angle);
static void RotateZ(Vertex3D *v, float angle);
static void Project(Vertex3D *v3d, Vertex2D *v2d);
static void Draw_Cube(void);
static float CalculateMidpointZ(const Vertex3D *v1, const Vertex3D *v2);

/**
  * @brief  应用入口
  */
void MPU6050_Enter(void)
{
    for(u8 i = 3; i < 12; i++) {
        OLED_OffsetUpdate(i, 0);
        Delay_ms(10);
    }
    
    OLED_ClearBuf();
    state = MPU6050_READY;
    
    Timer_SetPrescaler(menuOptions.SystemClock * 0.2);
    TaskTimer.UpdateTask = 1;
    
    ActionDisplay_Init(MPU6050_Display);
    ActionKey_Init(MPU6050_KeyAction);
}

/**
  * @brief  按键事件处理
  */
void MPU6050_KeyAction(uint8_t event)
{
    switch (state) {
    case MPU6050_READY:
        if (event == KEY_OK) { 
            state = MPU6050_PLAYING; 
        } else if (event == KEY_BACK) {
            ACTION_Exit();
        }
        break;

    case MPU6050_PLAYING:
        if (event == ENC_CW) {
            angle_y += 0.2f;
        } else if (event == ENC_CCW) {
            angle_y -= 0.2f;
        } else if (event == KEY_OK || event == KEY_BACK) {
            state = MPU6050_PAUSE;
        }
        break;
        
    case MPU6050_PAUSE:
        if (event == KEY_OK) {
            state = MPU6050_PLAYING;
        } else if (event == KEY_BACK) {
            ACTION_Exit();
        }
        break;
        
    case MPU6050_OVER:
        if (event == KEY_OK) {
            state = MPU6050_READY;
        } else if (event == KEY_BACK) {
            ACTION_Exit();
        }
        break;
    }
}

/**
  * @brief  显示回调
  */
void MPU6050_Display(void)
{
    OLED_ClearBuf();
    
    switch (state) {
    case MPU6050_READY:
        OLED_ShowString(10, 20, "Press OK", 0, 16, 8);
        OLED_ShowString(30, 40, "3D Cube", 0, 16, 8);
        break;

    case MPU6050_PLAYING:
        Show_MPU6050();
        break;
        
    case MPU6050_PAUSE:
        OLED_ShowString(50, 50, "PAUSE", 0);
        break;
        
    case MPU6050_OVER:
        OLED_ShowString(30, 30, "Over", 0, 16, 8);
        break;
    }
    
}

/**
  * @brief  绘制立方体主函数
  */
void Show_MPU6050(void)
{
    OLED_ClearBuf();
	
	angle_y+=0.02f;
	angle_x+=0.02f;
	angle_z+=0.04f;
	
	OLED_Printf(0,2,16,12,0,"X:%02d",(int)(angle_x*57.3));
	OLED_Printf(18,2,16,12,0,"Y:%02d",(int)(angle_y*57.3));
	OLED_Printf(36,2,16,12,0,"Z:%02d",(int)(angle_z*57.3));
	
    // 保持静止（仅手动旋转）
    if (angle_y > 2 * 3.14159f) angle_y = 0;
    if (angle_x > 2 * 3.14159f) angle_x = 0;
    if (angle_z > 2 * 3.14159f) angle_z = 0;
    
    Draw_Cube();
    


}

/**
  * @brief  计算边中点的Z坐标
  */
static float CalculateMidpointZ(const Vertex3D *v1, const Vertex3D *v2)
{
    return (v1->z + v2->z) * 0.5f;
}

/**
  * @brief  绕X轴旋转
  */
static void RotateX(Vertex3D *v, float angle)
{
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    float y = v->y * cos_a - v->z * sin_a;
    float z = v->y * sin_a + v->z * cos_a;
    v->y = y;
    v->z = z;
}

/**
  * @brief  绕Y轴旋转
  */
static void RotateY(Vertex3D *v, float angle)
{
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    float x = v->x * cos_a + v->z * sin_a;
    float z = -v->x * sin_a + v->z * cos_a;
    v->x = x;
    v->z = z;
}

/**
  * @brief  绕Z轴旋转
  */
static void RotateZ(Vertex3D *v, float angle)
{
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    float x = v->x * cos_a - v->y * sin_a;
    float y = v->x * sin_a + v->y * cos_a;
    v->x = x;
    v->y = y;
}

/**
  * @brief  3D到2D透视投影
  */
static void Project(Vertex3D *v3d, Vertex2D *v2d)
{
    float distance = 8.0f;
    float factor = distance / (distance + v3d->z);
    
    v2d->x = (int16_t)(v3d->x * factor * CUBE_SIZE + CENTER_X);
    v2d->y = (int16_t)(v3d->y * factor * CUBE_SIZE + CENTER_Y);
}

/**
  * @brief  绘制3D立方体（透视虚实线）
  */
static void Draw_Cube(void)
{
    Vertex3D rotated_vertices[8];
    Vertex2D projected_vertices[8];
    
    // 1. 旋转所有顶点
    for (int i = 0; i < 8; i++) {
        rotated_vertices[i] = cube_vertices[i];
        RotateX(&rotated_vertices[i], angle_x);
        RotateY(&rotated_vertices[i], angle_y);
        RotateZ(&rotated_vertices[i], angle_z);
    }
    
    // 2. 投影到2D
    for (int i = 0; i < 8; i++) {
        Project(&rotated_vertices[i], &projected_vertices[i]);
    }
    
    // 3. 绘制12条边（根据深度决定虚实）
    for (int i = 0; i < 12; i++) {
        // 计算边中点的Z坐标判断深度
        float mid_z = CalculateMidpointZ(
            &rotated_vertices[cube_edges[i][0]],
            &rotated_vertices[cube_edges[i][1]]
        );
        
        // Z > 0 表示在屏幕后方，用虚线绘制
        uint8_t is_dashed = (mid_z > 0.0f);
        
        OLED_DrawLine(
            projected_vertices[cube_edges[i][0]].x,
            projected_vertices[cube_edges[i][0]].y,
            projected_vertices[cube_edges[i][1]].x,
            projected_vertices[cube_edges[i][1]].y,
            is_dashed
        );
    }
}