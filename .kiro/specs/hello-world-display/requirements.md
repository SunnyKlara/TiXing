# 需求文档

## 简介

在 Raspberry Pi Pico 的 ST7789 LCD 显示屏（284x76 横屏模式）上显示 "Hello World" 文字。基于现有的 ST7789 驱动和 8x8 ASCII 字体系统，实现文字的居中显示。

## 术语表

- **Display_System**: ST7789 LCD 显示屏系统，通过 SPI(0) 总线通信，包含驱动、字体渲染和屏幕绘制功能
- **Screen**: 284x76 像素的横屏显示区域（rotation=1 模式），物理分辨率 76x284，通过自定义旋转表实现横屏
- **Font8x8**: 8x8 像素的 ASCII 点阵字体（Font8x8Complete 类），支持 ASCII 32-126 字符，WIDTH=8, HEIGHT=8
- **Hello_World_Screen**: 显示 "Hello World" 文字的界面
- **SPI_Bus**: SPI(0) 总线，baudrate=20000000, sck=Pin(2), mosi=Pin(3)
- **Control_Pins**: DC=Pin(0), CS=Pin(1), Reset=Pin(5)

## 需求

### 需求 1：显示 Hello World 文字

**用户故事：** 作为用户，我希望在 LCD 显示屏上看到 "Hello World" 文字，以便确认显示屏正常工作。

#### 验收标准

1. WHEN main.py 启动时，THE Display_System SHALL 在 Screen 上显示 "Hello World" 文字
2. THE Display_System SHALL 使用 Font8x8 字体渲染 "Hello World" 文字
3. THE Display_System SHALL 将 "Hello World" 文字在 Screen 的水平方向居中显示
4. THE Display_System SHALL 将 "Hello World" 文字在 Screen 的垂直方向居中显示
5. THE Display_System SHALL 使用白色（WHITE）作为文字颜色，黑色（BLACK）作为背景颜色

### 需求 2：Hello World 界面集成到现有界面流程

**用户故事：** 作为用户，我希望 Hello World 界面能替换现有的 LOGO 界面，以便在开机时看到 Hello World。

#### 验收标准

1. WHEN 程序启动时，THE Display_System SHALL 先清屏为黑色背景，再绘制 "Hello World" 文字
2. WHEN Hello World 界面显示完成后，THE Display_System SHALL 保持显示 2 秒后自动切换到模式选择界面
3. THE Display_System SHALL 将 Hello World 界面作为 LOGO 界面的替代，复用现有的 LOGO_SCREEN 状态常量

### 需求 3：文字居中计算

**用户故事：** 作为开发者，我希望文字位置能根据字符串长度和屏幕尺寸自动计算居中坐标，以便文字始终在屏幕中央。

#### 验收标准

1. THE Display_System SHALL 根据 "Hello World" 字符串长度（11个字符）和 Font8x8 字符宽度（8像素）计算水平居中坐标 X = (284 - 11*8) / 2 = 98
2. THE Display_System SHALL 根据 Font8x8 字符高度（8像素）计算垂直居中坐标 Y = (76 - 8) / 2 = 34
3. IF 计算出的坐标为负数，THEN THE Display_System SHALL 将坐标设为 0 以防止越界
