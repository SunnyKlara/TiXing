# 实现计划：Hello World 显示

## 概述

修改 `main.py` 中的 LOGO 界面常量，将显示文字从 "LOGO" 替换为 "Hello World"，居中坐标通过现有公式自动计算。

## 任务

- [x] 1. 修改 main.py 中的 LOGO_TEXT 常量
  - 将 `LOGO_TEXT = "LOGO"` 修改为 `LOGO_TEXT = "Hello World"`
  - `LOGO_X` 和 `LOGO_Y` 使用现有的计算公式，会自动更新为正确的居中坐标
  - 验证 `LOGO_X` 计算结果为 98，`LOGO_Y` 计算结果为 34
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2, 2.3, 3.1, 3.2_

- [ ]* 2. 编写居中坐标计算的属性测试
  - **Property 1: 居中坐标计算正确性**
  - **Validates: Requirements 1.3, 1.4, 3.1, 3.2, 3.3**
  - 使用 hypothesis 库在桌面 Python 环境中测试
  - 生成随机屏幕尺寸和文字长度，验证居中公式正确性
  - 包含边界情况：文字超出屏幕宽度时坐标钳制为 0

- [ ] 3. 最终检查点
  - 确认所有测试通过，如有问题请告知。
