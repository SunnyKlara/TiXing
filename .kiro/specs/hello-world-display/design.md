# 设计文档：Hello World 显示

## 概述

在现有的 MicroPython ST7789 LCD 控制项目中，将 LOGO 界面的显示文字从 "LOGO" 替换为 "Hello World"，并更新相关的居中坐标计算。该修改仅涉及 `main.py` 中的常量定义和 `draw_logo_screen()` 函数，不改变现有的界面流程和交互逻辑。

## 架构

现有架构完全满足需求，无需新增模块或组件：

```
main.py（界面控制主程序）
  ├── st7789py.py（ST7789 驱动，提供 text/fill/fill_rect 等方法）
  ├── lcdfont.py（8x8 ASCII 字体数据）
  ├── control.py（外设控制）
  └── key.py（编码器/按键输入）
```

修改范围仅限于 `main.py`，具体为：
1. 修改 `LOGO_TEXT` 常量值
2. 重新计算 `LOGO_X` 和 `LOGO_Y` 居中坐标

## 组件与接口

### 需修改的组件

**main.py 中的 LOGO 界面常量**：

当前代码：
```python
LOGO_TEXT = "LOGO"
LOGO_X = (SCREEN_WIDTH - len(LOGO_TEXT)*CHAR_WIDTH) // 2  # (284-4*8)//2 = 126
LOGO_Y = (SCREEN_HEIGHT - CHAR_HEIGHT) // 2               # (76-8)//2 = 34
```

修改后：
```python
LOGO_TEXT = "Hello World"
LOGO_X = (SCREEN_WIDTH - len(LOGO_TEXT)*CHAR_WIDTH) // 2  # (284-11*8)//2 = 98
LOGO_Y = (SCREEN_HEIGHT - CHAR_HEIGHT) // 2               # (76-8)//2 = 34（不变）
```

### 复用的现有接口

- `tft.text(font, text, x0, y0, color, background)` — ST7789 驱动的文字绘制方法
- `tft.fill(color)` — 全屏填充（清屏）
- `draw_logo_screen()` — 现有的 LOGO 界面绘制函数，无需修改函数体
- `switch_screen(LOGO_SCREEN)` — 现有的界面切换逻辑，无需修改

### 关键说明

`draw_logo_screen()` 函数体无需修改，因为它已经引用了 `LOGO_TEXT`、`LOGO_X`、`LOGO_Y` 常量：

```python
def draw_logo_screen():
    tft.fill(BACKGROUND_COLOR)
    tft.text(font8x8, LOGO_TEXT, LOGO_X, LOGO_Y, NORMAL_COLOR, BACKGROUND_COLOR)
```

修改常量值后，函数自动使用新的文字和坐标。

## 数据模型

本功能不涉及新的数据模型。使用的现有数据：

| 常量 | 类型 | 当前值 | 修改后值 | 说明 |
|------|------|--------|----------|------|
| `LOGO_TEXT` | str | `"LOGO"` | `"Hello World"` | 显示文字 |
| `LOGO_X` | int | 126 | 98 | 水平居中坐标 |
| `LOGO_Y` | int | 34 | 34 | 垂直居中坐标（不变） |
| `SCREEN_WIDTH` | int | 284 | 284（不变） | 屏幕宽度 |
| `SCREEN_HEIGHT` | int | 76 | 76（不变） | 屏幕高度 |
| `CHAR_WIDTH` | int | 8 | 8（不变） | 字符宽度 |
| `CHAR_HEIGHT` | int | 8 | 8（不变） | 字符高度 |


## 正确性属性

*属性是在系统所有有效执行中都应成立的特征或行为——本质上是关于系统应该做什么的形式化陈述。属性是人类可读规范与机器可验证正确性保证之间的桥梁。*

基于验收标准的 prework 分析，1.3/1.4（居中计算）是可测试的属性，3.1/3.2 是其具体实例（被属性覆盖），3.3 是边界情况（纳入属性测试的生成器中）。

**Property 1: 居中坐标计算正确性**

*对于任意*屏幕宽度、屏幕高度、字符宽度、字符高度和文字字符串，计算出的水平居中坐标应等于 `(screen_width - len(text) * char_width) // 2`，垂直居中坐标应等于 `(screen_height - char_height) // 2`。当计算结果为负数时，坐标应被钳制为 0。

**Validates: Requirements 1.3, 1.4, 3.1, 3.2, 3.3**

## 错误处理

本功能的错误场景极为有限，因为仅修改常量值：

1. **字符超出字体范围**：`tft.text()` 方法已内置检查 `font.FIRST <= ch < font.LAST`，超出范围的字符会被跳过。"Hello World" 中所有字符（包括空格）都在 ASCII 32-126 范围内，不会触发此问题。

2. **文字超出屏幕边界**："Hello World" 共 11 个字符 × 8 像素 = 88 像素，远小于屏幕宽度 284 像素，不会越界。`tft.text()` 方法内部也有 `x0 + font.WIDTH <= self.width` 的边界检查。

3. **坐标为负数**：当前计算结果 X=98, Y=34 均为正数。但设计中包含了负数钳制逻辑作为防御性编程。

## 测试策略

### 单元测试

由于本功能运行在 MicroPython 嵌入式环境中，传统的自动化单元测试受限。建议的验证方式：

- **常量值验证**：检查 `LOGO_TEXT == "Hello World"`、`LOGO_X == 98`、`LOGO_Y == 34`
- **视觉验证**：烧录后在实际硬件上确认文字显示正确且居中

### 属性测试

- **测试库**：由于 MicroPython 环境限制，属性测试需在桌面 Python 环境中运行，使用 `hypothesis` 库
- **最少迭代次数**：100 次
- **标签格式**：`Feature: hello-world-display, Property 1: 居中坐标计算正确性`

**Property 1 测试方案**：
- 生成随机的屏幕宽度（1-1000）、屏幕高度（1-1000）、字符宽度（1-32）、字符高度（1-32）和随机长度的文字字符串
- 验证居中计算公式 `max(0, (width - len(text) * char_w) // 2)` 的正确性
- 边界情况：文字长度超过屏幕宽度时，坐标应为 0
