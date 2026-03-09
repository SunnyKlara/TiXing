# 实施计划：F4 参考架构分析

## 概述

本实施计划将 f4 项目的架构分析和 Pico 项目的开发蓝图转化为可执行的任务序列。所有任务聚焦于创建分析文档和代码 demo，为后续正式开发提供参考。

## 任务

- [x] 1. Phase 0 - 架构重构：建立 f4 风格的界面状态机
  - [x] 1.1 创建 display.py 模块，将 main.py 中的所有界面绘制函数（draw_logo_screen、draw_mode_select_screen、draw_windspeed_screen、draw_smokespeed_screen、draw_rgbcontrol_screen）迁移到 display.py
    - display.py 接收 tft 实例和状态数据作为参数
    - main.py 只保留主循环、状态管理和交互逻辑
    - _Requirements: 5.3, 6.2_

  - [x] 1.2 重构 main.py 的界面状态机，实现 f4 风格的 ui/init_flag 双变量机制
    - 用 `ui` 变量控制当前界面编号，`init_flag` 控制一次性初始化
    - 界面切换时设置 `init_flag = True`，绘制函数检测到后执行初始化并重置
    - 参考 f4 的 chu 变量机制，避免每帧重绘整个界面
    - _Requirements: 5.1, 5.5, 7.1_

  - [x] 1.3 重构编码器处理，实现增量统一管理和鼠标滚轮适配
    - 输入设备是鼠标滚轮编码器（非旋钮），旋转连续且快速
    - 新增 `get_encoder_delta()` 函数：一次返回所有累积增量并清零（替代逐个消费的 get_encoder_dir）
    - 主循环开头调用一次，将 delta 作为参数传递给各界面处理函数
    - 菜单界面加 150ms 防跳（避免快速滚动跳过多个选项）
    - 调值界面支持加速：RGB 0-255 用 delta*2，风速/亮度 0-100 用 delta*1
    - 优化消抖策略，适配鼠标滚轮的抖动特性
    - _Requirements: 5.5, 7.2_

  - [x] 1.4 创建 config.py 配置持久化模块
    - 实现 `load_config()` 和 `save_config()` 函数，使用 JSON 文件存储
    - 实现脏标志机制（dirty flag），只在设置变更时标记，退出界面时写入
    - 配置项：fan_speed, smoke_speed, air_pump_speed, cob1_rgb, cob2_rgb, brightness, color_preset, led_mode
    - 参考 f4 的 deng_update()/deng_init() 和 WriteRead[] 数组机制
    - _Requirements: 5.3, 6.2, 7.5_

- [x] 2. Checkpoint - Phase 0 验证
  - 确保架构重构后所有现有功能正常工作（Logo→菜单→风速/发烟/RGB 控制）
  - 确保配置持久化可以保存和恢复设置
  - 请用户在 Pico 硬件上测试，确认无回归问题

- [x] 3. Phase 1 - 菜单系统升级和新界面
  - [x] 3.1 重新设计菜单界面（UI_MENU），实现 6 选项水平滚动菜单
    - 菜单项：风速、发烟、气泵、灯光预设、RGB调色、亮度
    - 76×284 横屏布局：当前选中项居中显示文字，左右滚动切换
    - 底部圆点指示器显示当前位置
    - 旋转水平滚动切换选项，单击进入子界面
    - 参考 f4 UI5 的 menu_selected 变量管理
    - _Requirements: 4.1, 5.2_

  - [x] 3.2 实现气泵控制界面（UI3），复用风速界面的进度条布局
    - 旋转调节 0-100%，双击返回菜单
    - 调用 control.py 的 set_air_pump_speed()
    - _Requirements: 4.1, 6.2_

  - [x] 3.3 实现灯光预设界面（UI4），参考 f4 UI2 的颜色预设切换
    - 12 种颜色预设（从 f4 的 streamlight_colors 数组移植）
    - 旋转切换预设，实时更新 COB 灯带颜色
    - 单击切换渐变模式开关
    - 双击保存并返回菜单
    - 界面显示预设编号和名称
    - _Requirements: 4.1, 4.2, 5.2_

  - [x] 3.4 重新实现 RGB 调色界面（UI5），参考 f4 UI3 的三层状态机
    - 状态机：选灯带(COB1/COB2) → 选通道(R/G/B) → 调数值(0-255)
    - 旋转在当前层级内切换/调值，单击进入下一层，双击返回菜单
    - 参考 f4 的 ui3_mode/ui3_channel/name 变量管理
    - _Requirements: 4.1, 4.4, 5.2_

  - [x] 3.5 实现亮度调节界面（UI6），参考 f4 UI4
    - 旋转调节全局亮度 0-100%
    - 单击切换呼吸灯模式开关（红点/绿点指示）
    - 双击保存并返回菜单
    - _Requirements: 4.1, 5.2_

- [x] 4. Checkpoint - Phase 1 验证
  - 确保所有 6 个子界面可以从菜单正常进入和退出
  - 确保每个界面的交互逻辑正确（旋转调值、单击进入、双击返回）
  - 请用户在 Pico 硬件上测试所有界面流程

- [x] 5. Phase 2 - 灯光效果系统
  - [x] 5.1 创建 effects.py 灯光效果引擎模块
    - 实现 LED 效果优先级系统：normal < preset_gradient < breathing
    - 实现 `effects_process()` 函数，在主循环中每帧调用
    - 参考 f4 的 LED_GradientProcess()/Streamlight_Process()/Breath_Process() 架构
    - 参考 f4 的 deng_2or3/breath_mode 标志位互斥机制
    - _Requirements: 4.4, 5.5, 7.3_

  - [x] 5.2 实现颜色预设渐变效果，参考 f4 的 Streamlight_Process()
    - 在 12 种预设之间平滑渐变过渡
    - 使用线性插值计算中间颜色
    - 控制渐变速度（参考 f4 的 30ms/帧）
    - 通过 control.py 的 set_cob1_rgb()/set_cob2_rgb() 输出
    - _Requirements: 4.2, 7.3_

  - [x] 5.3 实现呼吸灯效果，参考 f4 的 Breath_Process()
    - 使用正弦函数计算亮度变化（参考 f4 的 sinf(breath_phase * 0.01f)）
    - 呼吸周期约 3 秒（参考 f4 的 phase += 4, 628 步）
    - 退出亮度界面后呼吸灯持续运行（参考 f4 的 ui != 4 条件）
    - 进入 RGB 调色或预设界面时自动关闭呼吸灯（优先级互斥）
    - _Requirements: 4.4, 7.3_

  - [x] 5.4 实现开机动画效果（UI0），参考 f4 的 Startup_TaillightFlash()
    - Logo 显示 + COB 灯带闪烁动画
    - 渐亮到用户设置的颜色
    - 动画完成后自动进入菜单界面
    - _Requirements: 4.1_

- [x] 6. Checkpoint - Phase 2 验证
  - 确保灯光效果系统正常工作（预设渐变、呼吸灯、开机动画）
  - 确保效果优先级互斥正确（进入 RGB 调色时关闭呼吸灯）
  - 确保效果不影响主循环响应速度
  - 请用户在 Pico 硬件上测试灯光效果

- [x] 7. Phase 3 - 交互增强和配置持久化
  - [x] 7.1 增强 key.py 的按键事件系统，增加三击支持
    - 参考 f4 的 Check_KnobButton() 实现
    - 三击用于特殊功能（如进入油门模式）
    - 确保单击/双击/三击/长按的判定时序正确
    - _Requirements: 4.3, 7.1_

  - [x] 7.2 实现风速界面的油门模式，参考 f4 UI1 的 wuhuaqi_state==2
    - 三击进入油门模式
    - 油门模式下：按住加速，松开减速
    - 旋转退出油门模式
    - 参考 f4 的 Throttle_ResetState() 状态重置
    - _Requirements: 4.1, 4.3_

  - [x] 7.3 完善配置持久化，实现退出界面时自动保存
    - 双击返回菜单时触发 save_config()（仅在 dirty flag 为 True 时写入）
    - 开机时 load_config() 恢复上次设置
    - 保存项：风速、发烟速度、气泵速度、COB颜色、亮度、预设编号、LED模式
    - 参考 f4 的 deng_update() 写入时机
    - _Requirements: 6.2, 7.5_

- [x] 8. Final Checkpoint - 全功能验证
  - 确保所有界面、交互、灯光效果、配置持久化正常工作
  - 确保断电重启后配置正确恢复
  - 请用户在 Pico 硬件上进行完整功能测试
  - 确认是否需要创建后续 spec（UI 美化、蓝牙通信等）

## 说明

- 每个 Phase 结束后有 Checkpoint，确保增量验证
- 所有界面 UI 先用文字+进度条做 demo，后续再创建独立的 UI 美化 spec
- 蓝牙通信不在本计划范围内，需要时创建独立 spec
- 每个任务都标注了对应的需求编号，确保需求覆盖
- Phase 0 是地基，必须先完成；Phase 1-3 按顺序执行
