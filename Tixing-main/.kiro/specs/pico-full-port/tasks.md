# 实现计划：Pico 功能复刻（UI + 菜单 + 外设控制）

## 概述

基于已有的 Pico 项目代码（main.py, display.py, control.py, key.py, config.py, effects.py），增量实现 UI 增强、菜单复刻、设备联动和系统健壮性。每个任务在前一个任务基础上构建，确保代码始终可运行。

## 任务

- [x] 1. display.py 局部刷新基础设施
  - [x] 1.1 为速度控制界面添加 `update_windspeed_screen(tft, state, old_state)` 局部刷新函数
    - 仅重绘数字区域（先填充背景色矩形清除旧数字，再绘制新数字）和进度条区域
    - 添加 `_update_speed_number(tft, value, old_value)` 和 `_update_speed_bar(tft, value, throttle)` 辅助函数
    - _Requirements: 2.3, 7.2, 7.4_
  - [x] 1.2 为发烟和气泵界面添加局部刷新函数 `update_smokespeed_screen` 和 `update_airpump_screen`
    - 复用速度界面的 `_update_speed_number` 和 `_update_speed_bar` 辅助函数
    - _Requirements: 2.3, 7.2_
  - [x] 1.3 为亮度界面添加 `update_brightness_screen(tft, state, old_state)` 局部刷新函数
    - 重绘数字、进度条和呼吸灯状态指示
    - _Requirements: 5.4, 7.2_
  - [x] 1.4 为预设界面添加 `update_preset_screen(tft, state, old_state)` 局部刷新函数
    - 重绘预设编号、名称、色条预览和渐变模式指示
    - _Requirements: 7.2_
  - [x] 1.5 为 RGB 调色界面添加 `update_rgbcontrol_screen(tft, state, old_state)` 局部刷新函数
    - 重绘模式指示、通道高亮状态、数值和颜色预览
    - _Requirements: 7.2_
  - [x] 1.6 修改 main.py 的 `update_display()` 函数集成局部刷新
    - 添加 `_prev_state` 全局变量缓存上一帧状态
    - init_flag=True 时调用 `draw_xxx_screen`，init_flag=False 时调用 `update_xxx_screen`
    - 每帧结束时更新 `_prev_state = get_state()` 的深拷贝
    - _Requirements: 7.1, 7.2_
  - [ ]* 1.7 编写局部刷新策略的属性测试
    - **Property 4: 局部刷新策略正确性**
    - **Validates: Requirements 2.3, 5.4, 7.1, 7.2, 7.4**

- [x] 2. 菜单界面 UI 增强
  - [x] 2.1 增强 `draw_mode_select_screen` 添加值预览功能
    - 在选中项下方显示当前值（风速百分比、预设名称等）
    - 添加 `_draw_menu_value_preview(tft, idx, state)` 辅助函数
    - _Requirements: 1.3_
  - [x] 2.2 增强菜单界面添加操作提示
    - 在菜单界面显示 "Click:Enter" 提示文字
    - _Requirements: 6 (UI 增强)_
  - [x] 2.3 为所有子界面添加统一的操作提示
    - 添加 `_draw_hint(tft, text="2x:Back")` 辅助函数
    - 在每个 `draw_xxx_screen` 函数末尾调用
    - _Requirements: 2.6, 7 (子界面 UI 增强)_
  - [ ]* 2.4 编写菜单值预览一致性的属性测试
    - **Property 2: 菜单值预览与状态一致性**
    - **Validates: Requirements 1.3**

- [x] 3. Checkpoint - 确保所有测试通过
  - 确保所有测试通过，如有问题请询问用户。

- [x] 4. 菜单交互逻辑增强
  - [x] 4.1 验证并完善菜单防跳逻辑
    - 确认 main.py 中 150ms MENU_SCROLL_DEBOUNCE_MS 正常工作
    - 确保每次旋转仅移动一格（忽略 delta 大小）
    - _Requirements: 1.2_
  - [x] 4.2 验证菜单进入-返回往返一致性
    - 确认 `last_mode_idx` 在进入子界面时保存，返回时恢复到 `current_mode_idx`
    - _Requirements: 1.5, 1.6_
  - [ ]* 4.3 编写菜单进入-返回往返的属性测试
    - **Property 3: 菜单进入-返回往返一致性**
    - **Validates: Requirements 1.5, 1.6**

- [x] 5. 预设界面逻辑增强
  - [x] 5.1 增强预设界面显示预设编号格式
    - 显示 "N/12" 格式的编号（N = color_preset_idx + 1）
    - 确保预设名称与 COLOR_PRESETS 数组一致
    - _Requirements: 3.1_
  - [x] 5.2 增强预设界面纯色/渐变条自动选择
    - 在 `draw_preset_screen` 中检查 cob1 == cob2，选择 `draw_solid_bar` 或 `draw_gradient_bar`
    - _Requirements: 3.3_
  - [x] 5.3 验证预设切换环绕逻辑
    - 确认 `(color_preset_idx + encoder_delta) % len(COLOR_PRESETS)` 正确环绕
    - 确认切换时实时调用 `apply_preset_colors` 更新 COB 输出
    - _Requirements: 3.6_
  - [ ]* 5.4 编写预设切换环绕的属性测试
    - **Property 7: 预设切换环绕正确性**
    - **Validates: Requirements 3.6**

- [x] 6. RGB 调色界面逻辑增强
  - [x] 6.1 验证 RGB 数值调节范围约束
    - 确认 `max(0, min(255, vals[rgb_channel] + encoder_delta * 2))` 正确钳位
    - _Requirements: 4.5_
  - [x] 6.2 验证 RGB 状态机模式转换
    - 确认单击转换规则：0→1, 1→2, 2→1
    - _Requirements: 4.6_
  - [ ]* 6.3 编写 RGB 数值范围约束的属性测试
    - **Property 8: RGB 数值调节范围约束**
    - **Validates: Requirements 4.5**
  - [ ]* 6.4 编写 RGB 状态机转换的属性测试
    - **Property 9: RGB 状态机模式转换正确性**
    - **Validates: Requirements 4.6**

- [x] 7. Checkpoint - 确保所有测试通过
  - 确保所有测试通过，如有问题请询问用户。

- [x] 8. 油门模式增强
  - [x] 8.1 实现非线性加速曲线
    - 修改 main.py 油门模式逻辑：speed < 50 时 +1/帧，speed >= 50 时 +2/帧
    - 保持松开时 -1/帧 的线性减速
    - _Requirements: 8.1, 8.2_
  - [x] 8.2 验证油门模式旋转退出和界面重绘
    - 确认旋转时 throttle_mode 设为 False 并触发 init_flag = True
    - _Requirements: 8.3, 8.5_
  - [ ]* 8.3 编写油门模式属性测试
    - **Property 10: 油门模式非线性加速曲线**
    - **Property 11: 油门模式线性减速**
    - **Property 12: 油门模式旋转退出**
    - **Property 13: 风扇速度范围不变量**
    - **Validates: Requirements 8.1, 8.2, 8.3, 8.4**

- [x] 9. 设备联动协调器
  - [x] 9.1 创建 coordinator.py 模块
    - 实现 `DeviceCoordinator` 类：`on_fan_speed_change`、`on_smoke_change`、`process` 方法
    - 支持 auto/manual 联动模式
    - 实现气泵延迟 2 秒关闭的非阻塞定时逻辑
    - _Requirements: 9.1, 9.2, 9.3, 9.4_
  - [x] 9.2 集成 coordinator.py 到 main.py
    - 在主循环中实例化 DeviceCoordinator
    - 在风速变化时调用 `on_fan_speed_change`
    - 在发烟速度变化时调用 `on_smoke_change`
    - 每帧调用 `coordinator.process()`
    - _Requirements: 9.1, 9.2, 9.3, 9.5_
  - [ ]* 9.3 编写设备联动属性测试
    - **Property 14: 设备联动比例正确性**
    - **Property 15: 发烟-气泵联动正确性**
    - **Property 16: 手动模式独立控制**
    - **Validates: Requirements 9.1, 9.2, 9.3, 9.4**

- [x] 10. 配置持久化增强
  - [x] 10.1 增强 config.py 添加范围校验
    - 添加 `CONFIG_VALIDATORS` 字典定义每个配置项的有效范围
    - 在 `load_config()` 中对每个值进行校验，超出范围替换为默认值
    - 添加新配置项：`link_mode`、`fan_link_ratio`
    - _Requirements: 10.2, 10.4, 10.5_
  - [ ]* 10.2 编写配置加载健壮性的属性测试
    - **Property 17: 配置加载健壮性**
    - **Property 18: 脏标志写入优化**
    - **Validates: Requirements 10.2, 10.4, 10.5**

- [x] 11. 系统启动与错误恢复
  - [x] 11.1 增强 main.py 的启动序列和异常处理
    - 确保启动顺序：硬件引脚 → 配置加载 → 外设初始化 → 开机动画 → 菜单
    - 在主循环 try/except 中调用 stop_all_devices() 并打印错误信息
    - 添加主循环重启逻辑（可选：最多重试 3 次）
    - _Requirements: 10.1, 10.3_

- [x] 12. Final Checkpoint - 确保所有测试通过
  - 确保所有测试通过，如有问题请询问用户。

## 备注

- 标记 `*` 的任务为可选测试任务，可跳过以加快 MVP 进度
- 每个任务引用了具体的需求编号以确保可追溯性
- Checkpoint 任务确保增量验证
- 属性测试验证通用正确性属性，单元测试验证具体示例和边界情况
- 所有代码使用 MicroPython 兼容语法，属性测试在开发机上运行
