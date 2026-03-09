/**
  ******************************************************************************
  * @file    logo.h
  * @brief   自定义Logo上传与显示模块
  * @note    支持通过蓝牙上传240x240 RGB565格式图片到Flash
  *          存储地址: 0x100000 (W25Q128的1MB位置)
  ******************************************************************************
  */

#ifndef __LOGO_H
#define __LOGO_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

// ╔══════════════════════════════════════════════════════════════╗
// ║          Logo存储参数定义                                     ║
// ╚══════════════════════════════════════════════════════════════╝

// 单槽位参数
#define LOGO_WIDTH          240         // Logo宽度 (与APP端一致)
#define LOGO_HEIGHT         240         // Logo高度
#define LOGO_DATA_SIZE      (LOGO_WIDTH * LOGO_HEIGHT * 2)  // 115200字节
#define LOGO_HEADER_SIZE    16          // 头部信息大小
#define LOGO_TOTAL_SIZE     (LOGO_HEADER_SIZE + LOGO_DATA_SIZE)

#define LOGO_MAGIC          0xAA55      // 有效标志
#define LOGO_CHUNK_SIZE     200         // 每包数据大小 (Base64解码后)

// ╔══════════════════════════════════════════════════════════════╗
// ║          多槽位存储参数 (支持3张Logo)                         ║
// ╚══════════════════════════════════════════════════════════════╝

#define LOGO_MAX_SLOTS      3           // 最大槽位数
#define LOGO_SLOT_SIZE      0x20000     // 每槽位128KB

// 槽位Flash地址
#define LOGO_SLOT_0_ADDR    0x100000    // 槽位0地址 (1MB位置)
#define LOGO_SLOT_1_ADDR    0x120000    // 槽位1地址 (1.125MB位置)
#define LOGO_SLOT_2_ADDR    0x140000    // 槽位2地址 (1.25MB位置)
#define LOGO_CONFIG_ADDR    0x160000    // 配置存储地址 (1.375MB位置)

// 槽位地址计算宏
#define LOGO_SLOT_ADDR(slot) (LOGO_SLOT_0_ADDR + (slot) * LOGO_SLOT_SIZE)

// 配置有效标志
#define LOGO_CONFIG_MAGIC   0xBB66

// 向后兼容：默认槽位地址
#define LOGO_FLASH_ADDR     LOGO_SLOT_0_ADDR

// 默认Logo尺寸（154x154）- 保持原有默认图片尺寸
#define DEFAULT_LOGO_WIDTH  154
#define DEFAULT_LOGO_HEIGHT 154

// ╔══════════════════════════════════════════════════════════════╗
// ║          开机动画参数定义                                     ║
// ╚══════════════════════════════════════════════════════════════╝

#define ANIM_ENABLED        1           // 1=启用动画, 0=静态Logo
#define ANIM_SQUARE_SIZE    154         // 正方形边长（与原Logo大小一致）
#define ANIM_CENTER_X       120         // 屏幕中心X (240/2)
#define ANIM_CENTER_Y       120         // 屏幕中心Y (240/2)
#define ANIM_LINE_COLOR     0xFFFF      // 线条颜色 (白色)
#define ANIM_BG_COLOR       0x0000      // 背景颜色 (黑色)

// 动画时间参数 (毫秒) - 优化配合双闪+引擎声，总时长约1秒
#define ANIM_SQUARE_DURATION    300     // 正方形绘制时间
#define ANIM_DIAGONAL_DURATION  100     // 对角线绘制时间（快速延伸）
#define ANIM_CIRCLE_DURATION    200     // 圆形绘制时间（未使用，圆环一次性画出）
#define ANIM_HOLD_DURATION      500     // 最终保持时间（配合引擎声）
#define ANIM_FRAME_DELAY        10      // 每帧延时(ms)，约100fps

// 内切圆参数 (预计算)
// 圆与对角线外切，与下边框和右边框内切
// 半径 r = a / (2 + sqrt(2)) ≈ 45 (当 a = 154)
#define ANIM_CIRCLE_RADIUS      45      // 内切圆半径
// 圆心坐标 = 正方形右下角 - 半径
// x0 = 120 - 77 = 43, 所以 cx = 43 + 154 - 45 = 152
#define ANIM_CIRCLE_CX          152     // 内切圆圆心X
#define ANIM_CIRCLE_CY          152     // 内切圆圆心Y

// RLE解压缩缓冲区大小
#define DECOMPRESS_BUFFER_SIZE  2048    // 2KB解压缓冲区

// ╔══════════════════════════════════════════════════════════════╗
// ║          Logo头部结构                                         ║
// ╚══════════════════════════════════════════════════════════════╝

typedef struct {
    uint16_t magic;         // 有效标志 0xAA55
    uint16_t width;         // 图片宽度 240
    uint16_t height;        // 图片高度 240
    uint16_t reserved1;     // 保留
    uint32_t dataSize;      // 数据大小 115200
    uint32_t checksum;      // CRC32校验和
} LogoHeader_t;

// ╔══════════════════════════════════════════════════════════════╗
// ║          Logo配置结构 (多槽位管理)                            ║
// ╚══════════════════════════════════════════════════════════════╝

typedef struct {
    uint16_t magic;         // 配置有效标志 0xBB66
    uint8_t  active_slot;   // 当前激活的槽位 (0-2)
    uint8_t  reserved;      // 保留
    uint32_t checksum;      // 配置CRC32
} LogoConfig_t;

// ╔══════════════════════════════════════════════════════════════╗
// ║          Logo接收状态机                                       ║
// ╚══════════════════════════════════════════════════════════════╝

typedef enum {
    LOGO_STATE_IDLE = 0,        // 空闲状态
    LOGO_STATE_ERASING,         // 正在擦除Flash
    LOGO_STATE_RECEIVING,       // 正在接收数据
    LOGO_STATE_VERIFYING,       // 正在校验
    LOGO_STATE_COMPLETE,        // 传输完成
    LOGO_STATE_ERROR            // 错误状态
} LogoState_t;

// ╔══════════════════════════════════════════════════════════════╗
// ║          函数声明                                             ║
// ╚══════════════════════════════════════════════════════════════╝

// 初始化
void Logo_Init(void);

// 蓝牙命令处理 (在rx.c中调用)
void Logo_ParseCommand(char* cmd);

// 检查是否有有效的自定义Logo (默认槽位0)
bool Logo_IsValid(void);

// 显示自定义Logo到LCD
// 返回: true=显示成功, false=无有效Logo
bool Logo_ShowOnLCD(uint16_t x, uint16_t y);

// 开机Logo显示 (显示激活槽位的Logo，无则显示默认)
void Logo_ShowBoot(void);

// Logo界面显示 (优先自定义，否则默认)
void Logo_ShowCustom(void);

// 删除自定义Logo (默认槽位0)
void Logo_Delete(void);

// 获取当前传输状态
LogoState_t Logo_GetState(void);

// 获取传输进度 (0-100)
uint8_t Logo_GetProgress(void);

// 处理缓冲区 (在主循环中定期调用)
void Logo_ProcessBuffer(void);

// 检查是否有错误信息正在显示
bool Logo_IsErrorDisplayed(void);

// 清除错误显示标志
void Logo_ClearErrorFlag(void);

// ╔══════════════════════════════════════════════════════════════╗
// ║          多槽位管理函数                                       ║
// ╚══════════════════════════════════════════════════════════════╝

// 获取指定槽位的Flash地址
uint32_t Logo_GetSlotAddress(uint8_t slot);

// 检查指定槽位是否有有效Logo
bool Logo_IsSlotValid(uint8_t slot);

// 统计有效槽位数量
uint8_t Logo_CountValidSlots(void);

// 获取下一个有效槽位 (循环)
uint8_t Logo_NextValidSlot(uint8_t current);

// 获取上一个有效槽位 (循环)
uint8_t Logo_PrevValidSlot(uint8_t current);

// 显示指定槽位的Logo
bool Logo_ShowSlot(uint8_t slot);

// 设置激活槽位 (开机显示)
void Logo_SetActiveSlot(uint8_t slot);

// 获取激活槽位
uint8_t Logo_GetActiveSlot(void);

// 保存配置到Flash
void Logo_SaveConfig(void);

// 从Flash加载配置
void Logo_LoadConfig(void);

// 删除指定槽位的Logo
void Logo_DeleteSlot(uint8_t slot);

// 查找第一个空槽位
// 返回: 空槽位编号 (0-2), 无空槽位返回 LOGO_MAX_SLOTS
uint8_t Logo_FindEmptySlot(void);

// 获取自动上传目标槽位
// 优先返回空槽位，所有槽位都满时返回0（覆盖最旧的）
uint8_t Logo_GetAutoUploadSlot(void);

// ╔══════════════════════════════════════════════════════════════╗
// ║          开机动画函数                                         ║
// ╚══════════════════════════════════════════════════════════════╝

// 播放开机Logo动画
void Logo_PlayAnimation(void);

#endif /* __LOGO_H */
