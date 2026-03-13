# 菜单界面重构 - 设计文档

## 概述

本设计文档详细说明如何将现有的"长按循环切换UI"交互方式改为"菜单界面选择UI"的方式。

## 系统架构

### 当前架构

```
开机 → UI1 → 长按 → UI2 → 长按 → UI3 → 长按 → UI4 → 长按 → UI1（循环）
```

### 目标架构

```
开机 → 菜单(ui=5) → 单击 → UI1/UI2/UI3/UI4
                  ↑
                  └── 长按返回
```

## 状态机设计

### UI 状态定义

| ui 值 | 界面名称 | 功能描述 |
|-------|----------|----------|
| 1 | 风速调速 | 调节风扇速度，油门模式 |
| 2 | 预设配色 | 选择12种预设颜色方案 |
| 3 | 自定义调色 | RGB三通道独立调节 |
| 4 | 亮度调节 | 调节LED整体亮度 |
| 5 | **菜单界面** | 新增，选择进入哪个UI |

### chu 变量状态机

```
chu=5: 需要初始化菜单界面
chu=0: 菜单已初始化（或无需初始化）
chu=1: 需要初始化 UI1
chu=2: 需要初始化 UI2
chu=3: 需要初始化 UI3
chu=4: 需要初始化 UI4
```

### 界面切换状态转换

```c
// 开机初始化
ui=5, chu=5, menu_selected=1

// 从菜单进入 UI1（单击）
ui=5 → ui=1, chu=0 → chu=1

// 从 UI1 返回菜单（长按）
ui=1 → ui=5, chu=? → chu=5, menu_selected=1

// 从菜单进入 UI2（单击）
ui=5 → ui=2, chu=0 → chu=2

// 从 UI2 返回菜单（长按）
ui=2 → ui=5, chu=? → chu=5, menu_selected=2

// 油门模式退出到菜单（旋转/三击）
wuhuaqi_state=2 → wuhuaqi_state=saved, ui=5, chu=5, menu_selected=1
```

## 数据结构设计

### 新增全局变量

```c
// 在 xuanniu.h 中声明
extern uint8_t menu_selected;   // 菜单选中的 UI（1-4）
extern int16_t encoder_delta;   // 编码器增量（统一读取）
extern uint8_t auto_enter;      // 蓝牙命令自动进入标志

// 在 xuanniu.c 中定义
uint8_t menu_selected = 1;      // 默认选中 UI1
int16_t encoder_delta = 0;      // 每帧读取一次
uint8_t auto_enter = 0;         // 蓝牙触发自动进入
```

### 修改现有变量初始化

```c
// main.c 中的初始化
ui = 5;           // 开机进入菜单（原来是 ui=1）
chu = 5;          // 需要初始化菜单
menu_selected = 1; // 默认选中 UI1
```

## 模块设计

### 1. 编码器增量统一管理模块

**目标**：解决 `Encoder_GetDelta()` 被多处调用导致增量重复消费的问题。

**实现方案**：

```c
// xuanniu.c - Encoder() 函数开头
void Encoder()
{
    // 周期控制（20ms一次）
    if (uwTick - encoder_process_tick < 20) {
        return;
    }
    encoder_process_tick = uwTick;
    
    // ✅ 统一读取编码器增量（全局变量）
    encoder_delta = Encoder_GetDelta();
    
    // 后续代码使用 encoder_delta 而非重复调用 Encoder_GetDelta()
    // ...
}
```

**影响范围**：
- `Encoder()` 函数中的 UI1 逻辑
- `LCD()` 函数中的 UI2/UI3/UI4 逻辑
- `tiao_se()` 函数
- `deng_ui4()` 函数

### 2. 菜单界面显示模块

**布局设计**（240x240 圆形屏）：

```
+---------------------------+
|                           |
|    [速度]     [预设]      |
|     (40,60)   (140,60)    |
|                           |
|    [调色]     [亮度]      |
|     (40,140)  (140,140)   |
|                           |
+---------------------------+
```

**选中项高亮方式**：边框高亮（避免重绘图标）

```c
// 菜单项位置定义
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
```

### 3. 菜单界面交互逻辑

**Encoder() 中的菜单逻辑**：

```c
// 菜单界面的编码器处理
if (ui == 5) {
    // 旋转切换选中项
    if (encoder_delta != 0) {
        menu_selected += encoder_delta;
        // 循环边界处理
        if (menu_selected > 4) menu_selected = 1;
        if (menu_selected < 1) menu_selected = 4;
    }
    
    // 单击进入选中的 UI
    if (key_state == 1 && key_timeout) {
        ui = menu_selected;
        chu = menu_selected;
        key_state = 0;
    }
    
    // 长按在菜单界面不执行任何操作
}
```

**LCD() 中的菜单绘制**：

```c
if (ui == 5) {
    if (chu == 5) {
        // 初始化菜单界面
        LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
        Draw_Menu_Items();      // 绘制4个菜单项
        Draw_Menu_Border(menu_selected, 1);  // 绘制选中边框
        chu = 0;
    }
    
    // 更新选中项边框
    static uint8_t last_selected = 0;
    if (menu_selected != last_selected) {
        Draw_Menu_Border(last_selected, 0);   // 清除旧边框
        Draw_Menu_Border(menu_selected, 1);   // 绘制新边框
        last_selected = menu_selected;
    }
    
    // 处理蓝牙自动进入
    if (auto_enter && chu == 0) {
        ui = menu_selected;
        chu = menu_selected;
        auto_enter = 0;
    }
}
```

### 4. 返回菜单逻辑

**从功能界面返回菜单（双击）**：

```c
// 在 Encoder() 的双击处理中
else if(key_state == 2)//双击 - 返回菜单
{
    // 菜单界面：双击无操作
    if(ui == 5)
    {
        // 已在菜单中
    }
    // 油门模式：双击不返回菜单
    else if(wuhuaqi_state == 2)
    {
        // 油门模式使用旋转或三击退出
    }
    // 非菜单界面：双击返回菜单
    else
    {
        menu_selected = ui;  // 记住当前 UI
        ui = 5;
        chu = 5;
    }
}
```

### 5. 油门模式特殊处理

**退出油门模式到菜单**：

```c
// 在油门模式的退出逻辑中
if (wuhuaqi_state == 2) {
    // 旋转编码器退出
    if (delta != 0) {
        wuhuaqi_state = wuhuaqi_state_saved;
        ui = 5;           // 退出到菜单
        chu = 5;
        menu_selected = 1; // 选中 UI1
        // ... 其他清理逻辑
    }
    
    // 三击退出
    if (local_click_count >= 3) {
        wuhuaqi_state = wuhuaqi_state_saved;
        ui = 5;           // 退出到菜单
        chu = 5;
        menu_selected = 1; // 选中 UI1
        // ... 其他清理逻辑
    }
}
```

### 6. 蓝牙命令集成

**修改 rx.c 中的 UI:X 命令处理**：

```c
else if(strncmp(cmd, "UI:", 3) == 0)
{
    int target_ui = atoi(cmd + 3);
    if(target_ui >= 1 && target_ui <= 4)
    {
        // 先切换到菜单
        ui = 5;
        chu = 5;
        menu_selected = (uint8_t)target_ui;
        auto_enter = 1;  // 标志位：菜单初始化后自动进入
        
        printf("[BLE] UI SWITCH TO: %d via menu\r\n", target_ui);
    }
}
```

### 7. 子状态重置

**在 LCD() 的初始化逻辑中重置子状态**：

```c
if (ui == 2 && chu == 2) {
    pei_se_state = 0;  // 重置 UI2 子状态
    // ... 其他初始化
    chu = 3;
}

if (ui == 3 && chu == 3) {
    tiao_se_num = 0;   // 重置 UI3 子状态
    // ... 其他初始化
    chu = 4;
}

if (ui == 4 && chu == 4) {
    brt_state = 0;     // 重置 UI4 子状态
    // ... 其他初始化
    chu = 5;           // ✅ 修改：原来是 chu=1
}
```

## 函数接口设计

### 新增函数

```c
// lcd.c 中新增
void LCD_Menu_Init(void);                    // 菜单界面初始化
void Draw_Menu_Items(void);                  // 绘制菜单项
void Draw_Menu_Border(uint8_t item, uint8_t show);  // 绘制/清除边框

// xuanniu.c 中新增
void Menu_HandleEncoder(void);               // 菜单界面编码器处理
void Menu_HandleClick(void);                 // 菜单界面单击处理
```

### 修改函数

```c
// xuanniu.c
void Encoder(void);  // 添加菜单界面逻辑，统一编码器增量管理
void LCD(void);      // 添加 ui==5 的处理分支

// rx.c
void BLE_ParseCommand(char* cmd);  // 修改 UI:X 命令处理逻辑
```

## 测试要点

### 功能测试

1. **开机测试**：验证开机后显示菜单界面，默认选中 UI1
2. **旋转切换**：验证旋转编码器可循环切换选中项
3. **单击进入**：验证单击可进入选中的 UI
4. **长按返回**：验证各 UI 长按可返回菜单并记忆位置
5. **油门模式**：验证油门模式下长按不返回菜单，旋转/三击退出到菜单
6. **蓝牙切换**：验证 APP 发送 UI:X 命令可正确切换界面

### 边界测试

1. **menu_selected 边界**：验证 1→4→1 和 4→1→4 的循环
2. **快速旋转**：验证快速旋转不会跳过选项
3. **连续长按**：验证连续长按不会重复触发

### 兼容性测试

1. **UI1 功能**：验证风速调节、油门模式正常
2. **UI2 功能**：验证预设配色选择正常
3. **UI3 功能**：验证 RGB 调色正常
4. **UI4 功能**：验证亮度调节正常
5. **蓝牙通信**：验证所有蓝牙命令正常工作

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 编码器增量重复消费 | 旋转响应异常 | 统一在 Encoder() 开头读取一次 |
| chu 变量状态混乱 | 界面初始化失败 | 严格按状态机设计实现 |
| 油门模式冲突 | 无法正常加速 | 油门模式下禁用长按返回 |
| LCD 刷新闪烁 | 用户体验差 | 使用边框高亮，局部刷新 |

## 实现顺序

1. 添加全局变量和基础结构
2. 实现编码器增量统一管理
3. 实现菜单界面绘制
4. 实现菜单界面交互（旋转、单击）
5. 实现返回菜单功能
6. 处理油门模式特殊情况
7. 集成蓝牙命令
8. 测试和优化
