# 实施计划：Pico Turbo 全面重构

## 概述

从底层驱动到应用层，按六层架构依赖顺序逐步实现。每个任务构建在前序任务之上，确保增量可验证。目标平台为 RP2040 + MicroPython，所有代码使用 MicroPython 编写。

## 任务

- [x] 1. 项目骨架与基础设施
  - [x] 1.1 创建项目目录结构和所有 `__init__.py`
    - 创建 `drivers/`、`hal/`、`ui/`、`screens/`、`services/`、`assets/` 目录
    - 创建各目录的 `__init__.py` 文件
    - 创建 `ui/theme.py` 定义所有颜色常量（RGB565）、三区布局参数（顶栏 Y:0-11、主区 Y:12-55、底栏 Y:56-75）、12 种颜色预设 PRESETS 元组列表
    - _需求: 1.1, 1.5, 23.1, 23.2, 23.3, 23.4_

  - [x] 1.2 实现 AppState 全局状态类
    - 在项目根目录创建 `app_state.py`
    - 使用 `__slots__` 定义所有字段：fan_speed, smoke_speed, pump_speed, cob1_rgb, cob2_rgb, brightness, breath_mode, preset_idx, gradient_mode, throttle_mode, rgb_mode, rgb_strip, rgb_channel, menu_idx
    - 实现 `clamp()` 工具函数
    - _需求: 17.1, 17.2, 17.3, 17.4, 22.2_

- [x] 2. 底层驱动层（drivers/）
  - [x] 2.1 实现 ST7789 SPI 驱动
    - 创建 `drivers/st7789.py`
    - 实现 ST7789 类：SPI 初始化（GPIO0=DC, GPIO1=CS, GPIO2=SCK, GPIO3=MOSI, GPIO5=RST, 20MHz）
    - 实现 `init()` 初始化序列（rotation=1 横屏 284×76）
    - 实现 `set_window(x0, y0, x1, y1)` 设置写入区域
    - 实现 `write_data(buf)` 批量写入像素数据
    - 实现 `fill_rect(x, y, w, h, color)` 和 `fill(color)`
    - _需求: 2.1, 2.3, 2.5_

- [x] 3. 硬件抽象层（hal/）
  - [x] 3.1 实现 Display 显示封装
    - 创建 `hal/display.py`，封装 ST7789 驱动实例
    - 实现 `fill(color)`、`fill_rect(x,y,w,h,color)`、`blit(buf,x,y,w,h)` 三个核心接口
    - 实现 `hline()`、`vline()`、`pixel()` 辅助方法
    - 接受 RGB565 格式颜色值，blit() 使用 set_window + write_data 局部写入
    - _需求: 2.1, 2.2, 2.3, 2.4, 2.5_

  - [x] 3.2 实现 Input 编码器输入
    - 创建 `hal/input.py`
    - 定义事件常量 EVT_NONE/CLICK/DOUBLE/TRIPLE/LONG 和 InputEvent 数据类（`__slots__`: delta, key）
    - 实现 Input 类：GPIO 中断注册（A相=GPIO20, B相=GPIO21, 按键=GPIO19）
    - 实现旋转检测中断回调（2ms 消抖，累积 delta）
    - 实现按键状态机（20ms 消抖，300ms 多击窗口，1000ms 长按）
    - 实现 `poll()` 原子读取并清零，`raw_key_pressed()` 直接读取物理状态
    - _需求: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

  - [x] 3.3 实现 PwmDevices PWM 外设控制
    - 创建 `hal/pwm_devices.py`
    - 实现 PwmDevices 类，初始化所有 PWM 通道（1kHz）
    - 实现 `set_fan(pct)`、`set_small_fan(pct)`、`set_pump(pct)`、`set_smoke(pct)`，内部 clamp [0,100]，占空比 = pct × 65535 // 100
    - 实现 `set_cob1(r,g,b)`、`set_cob2(r,g,b)`，内部 clamp [0,255]
    - 实现 `stop_all()` 全部归零
    - 引脚映射：风扇=GPIO6, 小风扇=GPIO10, 气泵=GPIO11, 发烟器=GPIO12, COB1=GPIO13/14/15, COB2=GPIO7/8/9
    - _需求: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7_

  - [x]* 3.4 编写 PwmDevices 属性测试
    - **属性 3: ∀ speed ∈ [0,100]: set_fan(speed) 后 PWM duty = speed × 65535 / 100**
    - **属性 3 扩展: ∀ rgb ∈ [0,255]³: set_cob(r,g,b) 后各通道占空比正确**
    - 使用 hypothesis 在 PC 端测试 clamp 逻辑和占空比映射
    - **验证: 需求 4.4, 4.6**

- [x] 4. 检查点 — HAL 层完成
  - 确保所有 HAL 层模块无语法错误，可在 Pico 上 import 成功
  - 确保所有测试通过，如有疑问请询问用户

- [x] 5. 资源管理层（字体 + 位图）
  - [x] 5.1 实现 Font 字体引擎
    - 创建 `ui/font.py`
    - 实现 Font 类：从 .bin 文件加载字体（解析 4 字节头部：width, height, first_char, char_count）
    - 实现 `get_glyph(ch)` 返回 memoryview，`text_width(text)` 计算像素宽度
    - 实现 `get_font(size)` 缓存机制，支持 8/16/32 三种尺寸
    - 实现内置 8×8 回退字体 `_create_builtin_8px()`
    - _需求: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6_

  - [x]* 5.2 编写 Font 属性测试
    - **属性 10: ∀ font_size ∈ {8, 16, 32}: get_font(size) 返回有效 Font 实例**
    - 验证字体缓存一致性：同一 size 多次调用返回同一实例
    - **验证: 需求 5.5**

  - [x] 5.3 实现 Bitmap 位图加载器
    - 创建 `ui/bitmap.py`
    - 实现 Bitmap 类：从 .bin 文件加载 RGB565 位图（4 字节头部：width 2B LE + height 2B LE）
    - 实现 `blit_to(renderer, x, y)` 绘制到屏幕
    - 实现 IconSheet 类：加载图标集（6 字节头部：icon_w 2B + icon_h 2B + count 2B）
    - 实现 `blit_icon(renderer, index, x, y)` 绘制指定图标
    - _需求: 6.1, 6.2, 6.3, 6.4_

  - [x] 5.4 创建 PC 端资源生成工具
    - 创建 `tools/font_gen.py`：从 TTF 字体生成 .bin 位图字体文件（依赖 Pillow）
    - 创建 `tools/bitmap_gen.py`：将 PNG/SVG 转换为 RGB565 .bin 文件，支持单图和图标集模式
    - _需求: 5.7, 6.5_

- [x] 6. UI 框架层
  - [x] 6.1 实现 Renderer 渲染管线
    - 创建 `ui/renderer.py`
    - 实现 Renderer 类，分配 8 行行缓冲（284×8×2 = 4,544 字节）
    - 实现 `clear(color)`、`fill_rect(x,y,w,h,color)`、`blit_bitmap(data,x,y,w,h)` 基础接口
    - 实现 `draw_text(font,text,x,y,fg,bg)` 使用位图字体绘制文本，返回宽度
    - 实现 `draw_number(value,cx,cy,font,fg,bg)` 居中绘制大数字
    - 实现 `draw_hbar(x,y,w,h,pct,fg,bg)` 圆角胶囊进度条
    - 实现 `draw_gradient_bar(x,y,w,h,c1,c2)` 渐变色条
    - 实现 `draw_dot(cx,cy,r,color)` 实心圆点
    - _需求: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7_

  - [x]* 6.2 编写 Renderer 进度条属性测试
    - **属性 1: ∀ pct ∈ [0,100]: draw_hbar(pct) 前景宽度 ∈ [0, bar_width]**
    - 使用 hypothesis 验证进度条前景宽度与百分比的线性关系
    - **验证: 需求 7.4**

  - [x] 6.3 实现 widgets 可复用控件库
    - 创建 `ui/widgets.py`
    - 实现 `draw_capsule_bar()` 圆角胶囊进度条
    - 实现 `draw_gradient_bar()` 渐变胶囊色条
    - 实现 `draw_big_number()` 居中大数字显示
    - 实现 `draw_status_dot()` 状态指示圆点
    - 实现 `draw_nav_dots()` 底部导航圆点指示器
    - 实现 `draw_rgb_channel_bar()` RGB 通道条（标签+进度条+数值）
    - 实现 `draw_hint()` 底部操作提示文字
    - _需求: 24.1, 24.2, 24.3, 24.4, 24.5, 24.6, 24.7_

- [x] 7. 检查点 — UI 框架层完成
  - 确保 Renderer 和 widgets 可正确导入，所有测试通过
  - 确保所有测试通过，如有疑问请询问用户

- [x] 8. 界面状态机与 Screen 基类
  - [x] 8.1 实现 Screen 基类
    - 创建 `screens/__init__.py`，定义 Screen 基类
    - 定义五个生命周期方法：`on_enter()`、`on_input(event)`、`draw_full(renderer)`、`draw_update(renderer)`、`on_exit()`
    - 通过 `ctx` 上下文对象访问共享资源（renderer, devices, config, state, effects, coordinator, screen_manager）
    - _需求: 9.1, 9.2, 9.3, 9.4, 9.5_

  - [x] 8.2 实现 ScreenManager 界面状态机
    - 创建 `screen_manager.py`
    - 定义界面 ID 常量：UI_BOOT=0 到 UI_BRIGHT=7
    - 实现 `switch(ui_id)` 方法：调用旧界面 on_exit() → 更新 _current 和 _init_flag=True → 调用新界面 on_enter()
    - 实现 `render()` 方法：init_flag=True 时 clear()+draw_full() 并重置；否则 draw_update()
    - 实现 `dispatch(event)` 方法：转发输入事件给当前界面 on_input()
    - _需求: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6_

  - [x]* 8.3 编写 ScreenManager 属性测试
    - **属性 1: ∀ ui_id ∈ [0,7]: switch(ui_id) 后 current_ui == ui_id ∧ init_flag == True**
    - **属性 2: ∀ frame: init_flag == True → draw_full() 被调用 → init_flag 重置为 False**
    - 使用 hypothesis 验证界面切换状态一致性
    - **验证: 需求 8.2, 8.3, 8.4**

- [x] 9. 服务层（services/）
  - [x] 9.1 实现 Config 配置持久化
    - 创建 `services/config.py`
    - 实现 Config 类：JSON 格式读写 settings.json
    - 实现脏标志机制，仅在 `save()` 被调用且 dirty=True 时写入文件
    - 实现 `load_to_state(state)` 加载配置到 AppState，对每个值范围校验，超出范围用默认值替换
    - 实现 DEFAULT_CONFIG 默认值，文件损坏/缺失时使用默认值并输出警告
    - _需求: 18.1, 18.2, 18.3, 18.4, 18.5_

  - [x]* 9.2 编写 Config 属性测试
    - **属性 5: ∀ config: load(save(config)) == config（配置往返一致性）**
    - **属性 5 扩展: ∀ config_key: load_config() 后所有 key 都存在且值在有效范围内**
    - 使用 hypothesis 生成随机配置数据，验证序列化/反序列化一致性和范围校验
    - **验证: 需求 18.3, 18.4**

  - [x] 9.3 实现 Effects 灯光效果引擎
    - 创建 `services/effects.py`
    - 实现 Effects 类，支持三种模式：normal、breathing、gradient
    - 实现呼吸灯模式：使用 `math.sin()` 生成平滑亮度波动
    - 实现渐变模式：COB1 和 COB2 颜色之间线性插值循环
    - 实现 `process(state)` 每帧更新灯带输出
    - 实现开机动画 `startup_animation()`：COB 灯带交替闪烁 + 渐亮
    - 优先级：手动 RGB > 呼吸灯 > 渐变 > 普通
    - _需求: 19.1, 19.2, 19.3, 19.4, 19.5, 19.6_

  - [x] 9.4 实现 Coordinator 设备联动
    - 创建 `services/coordinator.py`
    - 实现 Coordinator 类
    - 实现风扇联动：主风扇变化时按比例（默认 0.8）同步小风扇
    - 实现发烟器联动：启动时自动启动气泵，关闭时延迟 2 秒非阻塞关闭气泵
    - 实现 `process()` 每帧处理延迟任务（非阻塞）
    - 支持手动模式下各设备独立控制
    - _需求: 20.1, 20.2, 20.3, 20.4, 20.5_

- [x] 10. 检查点 — 服务层完成
  - 确保 Config、Effects、Coordinator 可正确导入和基本功能正常
  - 确保所有测试通过，如有疑问请询问用户

- [x] 11. 开机动画界面（UI0）
  - [x] 11.1 实现 Boot 开机动画界面
    - 创建 `screens/boot.py`，继承 Screen 基类
    - `draw_full()` 中加载并居中显示 Logo 位图 + 品牌文字
    - 配合 Effects.startup_animation() 执行 COB 灯带开机动画（交替闪烁+渐亮）
    - 动画完成后自动调用 `ctx.screen_manager.switch(UI_MENU)` 切换到菜单
    - 确保上电到菜单总时长 ≤ 3 秒
    - 位图缺失时回退到纯文字 Logo
    - _需求: 16.1, 16.2, 16.3, 16.4, 25.2_

- [x] 12. 菜单界面（UI1）
  - [x] 12.1 实现 Menu 菜单界面
    - 创建 `screens/menu.py`，继承 Screen 基类
    - `draw_full()` 实现三栏布局：左侧小图标 | 中间大图标+标签+值预览 | 右侧小图标
    - `draw_update()` 仅重绘变化的图标和值预览区域
    - `on_input()` 处理旋转切换菜单项（6 项循环滚动），单击进入子界面
    - 底部使用 `draw_nav_dots()` 显示 6 个导航圆点
    - 中间区域显示选中功能的当前值预览（如 "50%"）
    - 从子界面返回时保持 menu_idx 不变
    - _需求: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6_

- [x] 13. 速度控制类界面（UI2/UI3/UI4）
  - [x] 13.1 实现 Speed 风速控制界面
    - 创建 `screens/speed.py`，继承 Screen 基类
    - 三区布局：顶栏 "WIND" 标题 + 主区 32px 大数字 + 底栏圆角胶囊进度条
    - `on_input()` 处理旋转调速（步进 1，clamp [0,100]）、双击保存返回菜单、三击进入油门模式
    - `draw_update()` 仅重绘数字和进度条区域（Partial_Refresh）
    - 实时同步 PWM 输出
    - _需求: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7_

  - [x] 13.2 实现油门模式逻辑
    - 在 `screens/speed.py` 中实现 Throttle_Mode
    - 三击激活，按住加速（<50 时 +1/帧，≥50 时 +2/帧），松开减速（-1/帧）
    - 旋转退出油门模式恢复普通调速
    - 油门模式下进度条切换为橙色警告色 + "THR" 标识
    - 风速始终 clamp [0,100]
    - _需求: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6_

  - [x]* 13.3 编写油门模式属性测试
    - **属性 7: ∀ throttle_mode == True: 旋转事件 → throttle_mode 重置为 False**
    - 验证油门模式下风速始终在 [0,100] 范围内
    - **验证: 需求 12.4, 12.5**

  - [x] 13.4 实现 Smoke 发烟控制界面
    - 创建 `screens/smoke.py`，继承 Screen 基类
    - 复用速度类布局：顶栏 "SMOKE" + 大数字 + 进度条
    - 旋转调速、双击保存返回，局部刷新
    - _需求: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7_

  - [x] 13.5 实现 Pump 气泵控制界面
    - 创建 `screens/pump.py`，继承 Screen 基类
    - 复用速度类布局：顶栏 "PUMP" + 大数字 + 进度条
    - 旋转调速、双击保存返回，局部刷新
    - _需求: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6, 11.7_

- [x] 14. 检查点 — 基础界面完成
  - 确保 Boot、Menu、Speed、Smoke、Pump 界面可正常切换和交互
  - 确保所有测试通过，如有疑问请询问用户

- [x] 15. 灯光预设界面（UI5）
  - [x] 15.1 实现 Preset 灯光预设界面
    - 创建 `screens/preset.py`，继承 Screen 基类
    - 顶栏显示预设编号（"3/12"）和预设名称
    - 主区显示渐变色条（COB1→COB2 渐变胶囊），COB1==COB2 时显示纯色
    - 底部显示 COB1/COB2 颜色对比小方块
    - 旋转切换预设（12 项循环），实时更新色条和 COB 硬件输出
    - 单击切换渐变模式开关，指示点绿/红 + "GRD"/"FIX" 标签
    - 双击保存返回菜单
    - _需求: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6, 13.7_

  - [x]* 15.2 编写预设索引属性测试
    - **属性 4: ∀ preset_idx ∈ [0,11]: PRESETS[preset_idx] 返回有效的 (name, cob1, cob2) 元组**
    - **验证: 需求 13.1**

- [x] 16. RGB 调色界面（UI6）
  - [x] 16.1 实现 RGB 三层状态机界面
    - 创建 `screens/rgb.py`，继承 Screen 基类
    - 实现三层状态机：mode=0 选灯带（COB1/COB2）→ mode=1 选通道（R/G/B）→ mode=2 调数值（0-255）
    - 顶栏显示模式指示（"Strip>"/"Chan>"/"Val>"）和选中灯带名称
    - 主区显示三行 R/G/B 通道：通道标签 + 彩色进度条 + 数值，选中通道高亮
    - 右上角显示当前灯带颜色实时预览色块
    - mode=2 时以 delta×2 步进调节，实时更新进度条和硬件输出
    - 单击推进状态机（0→1, 1→2, 2→1），双击重置 mode=0 保存返回菜单
    - _需求: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6, 14.7, 14.8_

  - [x]* 16.2 编写 RGB 状态机属性测试
    - **属性 6: ∀ rgb_mode ∈ {0,1,2}: 单击事件后 rgb_mode 转换正确（0→1, 1→2, 2→1）**
    - 验证 rgb_strip ∈ {0,1}，rgb_channel ∈ {0,1,2} 始终在有效范围
    - **验证: 需求 14.1, 14.7**

- [x] 17. 亮度调节界面（UI7）
  - [x] 17.1 实现 Brightness 亮度调节界面
    - 创建 `screens/brightness.py`，继承 Screen 基类
    - 32px 字体居中显示亮度百分比 + 底部圆角胶囊进度条
    - 呼吸灯状态指示点（绿=开/红=关）+ "Breath:ON"/"Breath:OFF" 文字
    - 旋转调节亮度（0-100），实时更新 COB 灯带亮度
    - 单击切换呼吸灯模式开关
    - 双击保存返回菜单，局部刷新数字和进度条
    - _需求: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6_

- [x] 18. 检查点 — 所有界面完成
  - 确保 Preset、RGB、Brightness 界面可正常切换和交互
  - 确保所有测试通过，如有疑问请询问用户

- [x] 19. 应用层集成与主循环
  - [x] 19.1 实现 main.py 应用入口
    - 创建 `main.py`
    - 按安全顺序初始化：PwmDevices.stop_all() → Display → Input → Config → AppState → Effects → Coordinator → Renderer → Context → ScreenManager
    - 实现开机动画流程：switch(UI_BOOT) → render() → startup_animation() → switch(UI_MENU)
    - 实现主循环：poll() → dispatch() → render() → effects.process() → coordinator.process() → sleep_ms(10)
    - RGB 界面时跳过 effects.process()
    - _需求: 21.1, 21.2, 25.6_

  - [x] 19.2 实现错误处理与安全恢复
    - 主循环 try/except 包裹，未捕获异常时 stop_all() + 打印错误 + 最多重试 3 次
    - KeyboardInterrupt 时 stop_all() 安全停止
    - MemoryError 时 gc.collect() + 释放字体缓存 + 重试，仍失败则降级最小 UI 模式
    - 字体/位图缺失时回退到内置 8×8 字体
    - _需求: 25.1, 25.2, 25.3, 25.4, 25.5_

  - [x]* 19.3 编写 ScreenManager 事件序列属性测试
    - **属性 5 (状态机): ∀ event sequence: screen_manager 状态始终在有效范围内**
    - **属性 8: ∀ 界面退出: on_exit() 在 switch() 中被调用**
    - **属性 9: ∀ 异常: stop_all_devices() 被调用，所有 PWM 占空比归零**
    - 使用 hypothesis 生成随机事件序列，验证状态机不变量
    - **验证: 需求 8.2, 8.5, 25.1**

- [x] 20. 最终检查点 — 全系统集成
  - 确保 main.py 可在 Pico 上正常启动，所有界面可切换
  - 确保所有测试通过，如有疑问请询问用户

## 备注

- 标记 `*` 的任务为可选测试任务，可跳过以加速 MVP 开发
- 每个任务标注了对应的需求编号，确保需求全覆盖
- 检查点任务用于阶段性验证，确保增量可靠
- 属性测试在 PC 端使用 pytest + hypothesis 运行，验证设计文档中的正确性属性
- 所有代码使用 MicroPython 编写，PC 端测试工具使用 CPython
