# 需求文档：Pico Turbo 全面重构

## 简介

本文档定义了 Pico Turbo 嵌入式项目从零重构的完整需求。项目目标是在 Raspberry Pi Pico (RP2040 + MicroPython) 平台上，使用 ST7789 76×284 横屏和 EC10 旋转编码器，构建一套专业级 UI 和完整外设控制系统。对标 F4 参考项目（STM32F405 + 240×240 圆屏）的功能和交互品质，但完全重新设计架构和视觉方案。

现有代码全部废弃，从零开始。核心挑战：在 264KB RAM / 2MB Flash 的严格资源约束下，实现位图字体渲染、局部刷新、多界面状态机、灯光效果引擎和多设备联动。

## 术语表

- **Pico_System**: Raspberry Pi Pico 上运行的 MicroPython 主控系统
- **Screen_Manager**: 界面状态机，基于 ui/init_flag 双变量控制界面切换和刷新（对标 F4 的 ui/chu 机制）
- **Renderer**: 渲染管线，管理行缓冲和屏幕写入
- **Font_Engine**: 位图字体引擎，从 .bin 文件加载预渲染字形
- **Bitmap_Loader**: RGB565 位图加载器，从文件读取图标和 Logo
- **Effects_Engine**: 灯光效果引擎，管理呼吸灯、渐变等 COB 灯带效果
- **Device_Coordinator**: 设备联动协调器，管理风扇/气泵/发烟器的自动联动
- **AppState**: 全局应用状态对象，所有界面共享读写
- **Partial_Refresh**: 局部刷新，仅重绘变化区域而非全屏
- **Throttle_Mode**: 油门模式，按住编码器按键加速、松开减速
- **Color_Preset**: 12 种预定义灯带颜色组合
- **HAL**: 硬件抽象层，封装底层硬件访问

## 需求

### 需求 1：六层项目架构

**用户故事：** 作为开发者，我希望项目有清晰的分层架构和模块边界，以便代码可维护、可测试、可扩展。

#### 验收标准

1. THE Pico_System SHALL 采用六层架构：驱动层(drivers/) → 硬件抽象层(hal/) → 资源层(ui/font+bitmap) → UI框架层(ui/+screens/) → 服务层(services/) → 应用层(main.py+screen_manager.py)
2. THE Pico_System SHALL 确保上层模块仅依赖下层模块，禁止反向依赖或跨层直接调用
3. THE Pico_System SHALL 将每个功能界面实现为独立文件（screens/*.py），继承统一的 Screen 基类
4. THE Pico_System SHALL 将所有硬件引脚访问封装在 hal/ 层，UI 层和服务层不直接操作 machine 模块
5. THE Pico_System SHALL 将所有可复用 UI 控件封装在 ui/widgets.py 中，界面文件通过调用控件函数绘制
6. THE Pico_System SHALL 将配置持久化、灯光效果、设备联动封装在 services/ 层，与 UI 层解耦


### 需求 2：ST7789 显示驱动封装

**用户故事：** 作为开发者，我希望显示驱动被正确封装，以便 UI 层无需关心 SPI 通信细节。

#### 验收标准

1. THE HAL SHALL 提供 Display 类，封装 ST7789 SPI 驱动（GPIO0=DC, GPIO1=CS, GPIO2=SCK, GPIO3=MOSI, GPIO5=RST）
2. THE Display SHALL 提供 fill(color)、fill_rect(x,y,w,h,color)、blit(buf,x,y,w,h) 三个核心接口
3. THE Display SHALL 以 20MHz SPI 时钟驱动 ST7789，屏幕分辨率 284×76（rotation=1 横屏）
4. THE Display SHALL 接受 RGB565 格式的颜色值和缓冲区数据
5. THE Display SHALL 在 blit() 中使用 set_window + write_data 实现局部区域写入，避免全屏传输

### 需求 3：EC10 编码器输入系统

**用户故事：** 作为用户，我希望旋转编码器的操作响应灵敏且准确，以便流畅地导航和调节参数。

#### 验收标准

1. THE HAL SHALL 提供 Input 类，封装 EC10 编码器（GPIO19=按键, GPIO20=A相, GPIO21=B相）
2. THE Input SHALL 通过 GPIO 中断检测旋转方向，累积增量值，poll() 时原子读取并清零
3. THE Input SHALL 实现按键状态机，支持五种事件：无事件(EVT_NONE)、单击(EVT_CLICK)、双击(EVT_DOUBLE)、三击(EVT_TRIPLE)、长按(EVT_LONG)
4. THE Input SHALL 对编码器旋转实施 2ms 最小间隔消抖，对按键实施 20ms 消抖
5. THE Input SHALL 使用 300ms 超时窗口判定多击，1000ms 阈值判定长按
6. THE Input SHALL 提供 raw_key_pressed() 方法直接读取按键物理状态（油门模式专用）
7. THE Input SHALL 通过 InputEvent 数据类返回事件，包含 delta（编码器增量）和 key（按键事件类型）

### 需求 4：PWM 外设统一控制

**用户故事：** 作为开发者，我希望所有 PWM 外设有统一的百分比控制接口，以便简化上层调用。

#### 验收标准

1. THE HAL SHALL 提供 PwmDevices 类，统一管理所有 PWM 外设
2. THE PwmDevices SHALL 提供 set_fan(pct)、set_small_fan(pct)、set_pump(pct)、set_smoke(pct) 方法，参数为 0-100 整数百分比
3. THE PwmDevices SHALL 提供 set_cob1(r,g,b)、set_cob2(r,g,b) 方法，RGB 各通道 0-255
4. THE PwmDevices SHALL 在所有 set_xxx() 方法内部对参数进行 clamp，防止越界
5. THE PwmDevices SHALL 提供 stop_all() 方法，将所有 PWM 占空比归零
6. THE PwmDevices SHALL 使用 1kHz PWM 频率，占空比映射：百分比 × 65535 / 100
7. THE PwmDevices SHALL 正确映射引脚：风扇=GPIO6, 小风扇=GPIO10, 气泵=GPIO11, 发烟器=GPIO12, COB1=GPIO13/14/15, COB2=GPIO7/8/9


### 需求 5：位图字体渲染系统

**用户故事：** 作为用户，我希望界面文字清晰美观，告别现有的粗糙像素风格。

#### 验收标准

1. THE Font_Engine SHALL 从 .bin 文件加载预渲染位图字体，支持 16px 和 32px 两种尺寸
2. THE Font_Engine SHALL 解析字体文件头部（width, height, first_char, char_count 各 1 字节）和列优先位图字形数据
3. THE Font_Engine SHALL 提供 get_glyph(ch) 方法返回单个字符的 memoryview，避免数据拷贝
4. THE Font_Engine SHALL 提供 text_width(text) 方法计算文本像素宽度
5. THE Font_Engine SHALL 实现字体缓存机制（get_font(size)），同一尺寸字体仅加载一次
6. THE Font_Engine SHALL 保留内置 8×8 像素字体作为回退方案，当 .bin 文件缺失时自动降级
7. THE Pico_System SHALL 提供 PC 端字体生成工具（tools/font_gen.py），从 TTF 字体生成 .bin 文件

### 需求 6：位图资源系统

**用户故事：** 作为用户，我希望界面使用精美的图标和 Logo，而非代码绘制的简陋线条图形。

#### 验收标准

1. THE Bitmap_Loader SHALL 从 .bin 文件加载 RGB565 位图，文件格式为 4 字节头部（width 2B LE + height 2B LE）+ 像素数据
2. THE Bitmap_Loader SHALL 提供 Bitmap 类用于加载单张位图，提供 blit_to(renderer, x, y) 方法绘制到屏幕
3. THE Bitmap_Loader SHALL 提供 IconSheet 类用于加载图标集（多个等尺寸图标打包），提供 blit_icon(renderer, index, x, y) 方法绘制指定图标
4. THE IconSheet SHALL 解析 6 字节头部（icon_w 2B + icon_h 2B + count 2B）
5. THE Pico_System SHALL 提供 PC 端位图生成工具（tools/bitmap_gen.py），将 PNG/SVG 转换为 RGB565 .bin 文件
6. THE Pico_System SHALL 在 assets/ 目录存放资源文件：font_16.bin、font_32.bin、icons_24.bin（6 个 24×24 菜单图标）、logo.bin

### 需求 7：渲染管线

**用户故事：** 作为开发者，我希望有统一的渲染接口，以便界面代码无需直接操作显示驱动。

#### 验收标准

1. THE Renderer SHALL 提供 clear(color)、fill_rect(x,y,w,h,color)、blit_bitmap(data,x,y,w,h) 基础绘制接口
2. THE Renderer SHALL 提供 draw_text(font,text,x,y,fg,bg) 方法使用位图字体绘制文本，返回绘制宽度
3. THE Renderer SHALL 提供 draw_number(value,cx,cy,font,fg,bg) 方法居中绘制大数字
4. THE Renderer SHALL 提供 draw_hbar(x,y,w,h,pct,fg,bg) 方法绘制圆角胶囊进度条
5. THE Renderer SHALL 提供 draw_gradient_bar(x,y,w,h,c1,c2) 方法绘制渐变色条
6. THE Renderer SHALL 提供 draw_dot(cx,cy,r,color) 方法绘制实心圆点
7. THE Renderer SHALL 分配 8 行行缓冲（284×8×2 = 4,544 字节），用于位图渲染的中间缓冲


### 需求 8：界面状态机（Screen Manager）

**用户故事：** 作为开发者，我希望界面切换有统一的状态管理机制，以便所有界面遵循一致的生命周期。

#### 验收标准

1. THE Screen_Manager SHALL 管理 8 个界面：开机(UI_BOOT=0)、菜单(UI_MENU=1)、风速(UI_SPEED=2)、发烟(UI_SMOKE=3)、气泵(UI_PUMP=4)、预设(UI_PRESET=5)、RGB(UI_RGB=6)、亮度(UI_BRIGHT=7)
2. THE Screen_Manager SHALL 实现 ui/init_flag 双变量机制：switch(ui_id) 设置 _current=ui_id 且 _init_flag=True
3. WHEN render() 被调用且 _init_flag=True 时，THE Screen_Manager SHALL 调用 clear() + screen.draw_full()，然后重置 _init_flag=False
4. WHEN render() 被调用且 _init_flag=False 时，THE Screen_Manager SHALL 仅调用 screen.draw_update()
5. WHEN switch() 被调用时，THE Screen_Manager SHALL 先调用旧界面的 on_exit()，再调用新界面的 on_enter()
6. THE Screen_Manager SHALL 通过 dispatch(event) 将输入事件转发给当前界面的 on_input() 方法

### 需求 9：Screen 基类协议

**用户故事：** 作为开发者，我希望所有界面遵循统一的接口协议，以便状态机能一致地管理它们。

#### 验收标准

1. THE Screen 基类 SHALL 定义四个生命周期方法：on_enter()、on_input(event)、draw_full(renderer)、draw_update(renderer)、on_exit()
2. THE Screen 基类 SHALL 通过 ctx 上下文对象访问共享资源（renderer, devices, config, state, effects, coordinator, screen_manager）
3. WHEN draw_full() 被调用时，THE Screen SHALL 绘制界面的所有视觉元素（清屏后的完整绘制）
4. WHEN draw_update() 被调用时，THE Screen SHALL 仅重绘自上次绘制以来发生变化的区域
5. THE Screen SHALL 在 on_input() 中处理编码器旋转和按键事件，更新 AppState 并可触发界面切换

### 需求 10：菜单界面（UI1）

**用户故事：** 作为用户，我希望菜单导航流畅直观，能快速找到并进入目标功能。

#### 验收标准

1. WHEN 菜单界面显示时，THE Pico_System SHALL 以三栏布局展示：左侧小图标 | 中间大图标+标签+值预览 | 右侧小图标
2. WHEN 编码器旋转时，THE Pico_System SHALL 切换菜单选中项，支持 6 个功能项循环滚动
3. THE Pico_System SHALL 在菜单底部显示 6 个导航圆点，选中项为强调色实心点，其余为灰色
4. WHEN 选中项变化时，THE Pico_System SHALL 在中间区域显示该功能的当前值预览（如风速 "50%"）
5. WHEN 用户单击编码器时，THE Pico_System SHALL 进入选中项对应的子界面
6. WHEN 用户从子界面返回时，THE Pico_System SHALL 保持菜单选中项为上次进入的位置

### 需求 11：速度控制类界面（UI2 风速 / UI3 发烟 / UI4 气泵）

**用户故事：** 作为用户，我希望速度控制界面有统一的视觉风格和清晰的数值显示。

#### 验收标准

1. WHEN 速度控制界面显示时，THE Pico_System SHALL 采用三区布局：顶栏(12px 标题) + 主区(44px 大数字) + 底栏(20px 进度条)
2. THE Pico_System SHALL 使用 32px 位图字体居中显示当前百分比值（0-100）
3. THE Pico_System SHALL 在底部显示圆角胶囊进度条（灰色背景 + 强调色前景）
4. WHEN 数值变化时，THE Pico_System SHALL 仅重绘数字区域和进度条区域（Partial_Refresh）
5. THE Pico_System SHALL 在顶栏显示功能标题（"WIND" / "SMOKE" / "PUMP"）
6. WHEN 编码器旋转时，THE Pico_System SHALL 以步进 1 调节百分比值，clamp 到 [0, 100]
7. WHEN 用户双击编码器时，THE Pico_System SHALL 保存配置并返回菜单界面


### 需求 12：油门模式（风速界面专属）

**用户故事：** 作为用户，我希望油门模式提供按住加速、松开减速的直觉操控体验。

#### 验收标准

1. WHEN 用户在风速界面三击编码器时，THE Pico_System SHALL 激活 Throttle_Mode
2. WHILE Throttle_Mode 激活且按键按下时，THE Pico_System SHALL 以非线性加速递增风速：速度 < 50 时每帧 +1，速度 ≥ 50 时每帧 +2
3. WHILE Throttle_Mode 激活且按键释放时，THE Pico_System SHALL 以每帧 -1 递减风速
4. WHEN Throttle_Mode 下编码器旋转时，THE Pico_System SHALL 退出油门模式并恢复普通调速
5. THE Pico_System SHALL 确保油门模式下风速始终 clamp 到 [0, 100]
6. WHEN Throttle_Mode 激活时，THE Pico_System SHALL 将进度条颜色切换为警告色（橙色）并显示 "THR" 标识

### 需求 13：灯光预设界面（UI5）

**用户故事：** 作为用户，我希望灯光预设界面能直观展示颜色效果，快速选择喜欢的方案。

#### 验收标准

1. THE Pico_System SHALL 提供 12 种颜色预设，每种包含名称、COB1 RGB 值、COB2 RGB 值
2. WHEN 预设界面显示时，THE Pico_System SHALL 在顶栏显示预设编号（如 "3/12"）和预设名称
3. WHEN 预设界面显示时，THE Pico_System SHALL 在主区显示渐变色条（COB1 颜色渐变到 COB2 颜色的胶囊形色条）
4. WHEN COB1 和 COB2 颜色相同时，THE Pico_System SHALL 显示纯色胶囊条
5. THE Pico_System SHALL 在底部显示 COB1 和 COB2 的颜色对比小方块
6. WHEN 编码器旋转切换预设时，THE Pico_System SHALL 实时更新色条预览和 COB 灯带硬件输出
7. WHEN 用户单击编码器时，THE Pico_System SHALL 切换渐变模式开关，通过指示点颜色（绿/红）和标签（"GRD"/"FIX"）反映状态

### 需求 14：RGB 调色界面（UI6）

**用户故事：** 作为用户，我希望 RGB 调色界面能精确调节灯带颜色，三层状态机操作清晰。

#### 验收标准

1. THE Pico_System SHALL 实现三层状态机：mode=0 选灯带（COB1/COB2）→ mode=1 选通道（R/G/B）→ mode=2 调数值（0-255）
2. WHEN RGB 界面显示时，THE Pico_System SHALL 在顶栏显示模式指示（"Strip>"/"Chan>"/"Val>"）和选中灯带名称
3. THE Pico_System SHALL 显示三行 R/G/B 通道，每行包含通道标签、彩色进度条和数值
4. WHEN 选中通道变化时，THE Pico_System SHALL 将选中通道高亮显示，未选中通道暗色显示
5. THE Pico_System SHALL 在顶部右侧显示当前灯带颜色的实时预览色块
6. WHEN mode=2 调数值时，THE Pico_System SHALL 以 delta×2 步进调节（0-255 范围），实时更新进度条和硬件输出
7. WHEN 单击编码器时，THE Pico_System SHALL 推进状态机（0→1, 1→2, 2→1）
8. WHEN 双击编码器时，THE Pico_System SHALL 重置 mode=0 并保存返回菜单

### 需求 15：亮度调节界面（UI7）

**用户故事：** 作为用户，我希望亮度调节界面清晰显示当前亮度和呼吸灯状态。

#### 验收标准

1. WHEN 亮度界面显示时，THE Pico_System SHALL 使用 32px 字体居中显示亮度百分比
2. THE Pico_System SHALL 在底部显示圆角胶囊亮度进度条
3. WHEN 呼吸灯模式变化时，THE Pico_System SHALL 通过指示点（绿色=开/红色=关）和文字（"Breath:ON"/"Breath:OFF"）反映状态
4. WHEN 编码器旋转时，THE Pico_System SHALL 调节亮度值（0-100），实时更新 COB 灯带亮度
5. WHEN 用户单击编码器时，THE Pico_System SHALL 切换呼吸灯模式开关
6. WHEN 亮度值变化时，THE Pico_System SHALL 仅重绘数字和进度条区域（Partial_Refresh）


### 需求 16：开机动画（UI0）

**用户故事：** 作为用户，我希望开机时有专业的视觉效果，体现产品品质。

#### 验收标准

1. WHEN Pico_System 上电启动时，THE Pico_System SHALL 显示 Logo 位图居中 + 品牌文字
2. WHEN Logo 界面显示时，THE Pico_System SHALL 同步执行 COB 灯带开机动画（交替闪烁 + 渐亮）
3. WHEN 开机动画完成后，THE Pico_System SHALL 自动切换到菜单界面
4. THE Pico_System SHALL 确保从上电到菜单界面的总时长不超过 3 秒

### 需求 17：全局应用状态管理

**用户故事：** 作为开发者，我希望全局状态集中管理，避免散落的全局变量。

#### 验收标准

1. THE Pico_System SHALL 使用 AppState 类集中管理所有运行时状态，使用 __slots__ 优化内存
2. THE AppState SHALL 包含：fan_speed(0-100)、smoke_speed(0-100)、pump_speed(0-100)、cob1_rgb([0-255]×3)、cob2_rgb([0-255]×3)、brightness(0-100)、breath_mode(bool)、preset_idx(0-11)、gradient_mode(bool)、throttle_mode(bool)、rgb_mode(0-2)、rgb_strip(0-1)、rgb_channel(0-2)、menu_idx(0-5)
3. THE Pico_System SHALL 确保所有界面通过 ctx.state 访问 AppState，不使用模块级全局变量
4. THE Pico_System SHALL 对 AppState 中的所有数值字段实施范围校验

### 需求 18：配置持久化

**用户故事：** 作为用户，我希望调节的参数在断电后能保存，下次开机恢复。

#### 验收标准

1. THE Pico_System SHALL 使用 JSON 格式将配置保存到 Flash 文件系统（settings.json）
2. THE Pico_System SHALL 实现脏标志机制，仅在配置实际变更时写入文件，减少 Flash 擦写
3. IF 配置文件损坏或缺失，THEN THE Pico_System SHALL 使用 DEFAULT_CONFIG 默认值并输出警告
4. WHEN 加载配置时，THE Pico_System SHALL 对每个值进行范围校验，超出范围的值替换为默认值
5. THE Pico_System SHALL 在用户主动保存时（双击返回菜单）触发配置写入，而非每次修改都写入

### 需求 19：灯光效果引擎

**用户故事：** 作为用户，我希望灯带有呼吸灯和渐变等动态效果。

#### 验收标准

1. THE Effects_Engine SHALL 支持三种灯光模式：普通(normal)、呼吸灯(breathing)、渐变(gradient)
2. WHEN 呼吸灯模式激活时，THE Effects_Engine SHALL 使用 sin() 函数生成平滑的亮度波动
3. WHEN 渐变模式激活时，THE Effects_Engine SHALL 在 COB1 和 COB2 颜色之间进行线性插值循环
4. THE Effects_Engine SHALL 在每帧主循环中通过 process(state) 更新灯带输出
5. WHEN 当前界面为 RGB 调色界面时，THE Pico_System SHALL 跳过效果引擎处理，避免与手动调色冲突
6. THE Effects_Engine SHALL 遵循优先级：手动 RGB 调色 > 呼吸灯 > 渐变 > 普通

### 需求 20：设备联动

**用户故事：** 作为用户，我希望相关设备能自动联动，减少手动操作。

#### 验收标准

1. WHEN 主风扇速度变化时，THE Device_Coordinator SHALL 按可配置比例（默认 0.8）同步调整小风扇速度
2. WHEN 发烟器启动时，THE Device_Coordinator SHALL 自动启动气泵辅助烟雾扩散
3. WHEN 发烟器关闭时，THE Device_Coordinator SHALL 延迟 2 秒后关闭气泵（非阻塞）
4. THE Device_Coordinator SHALL 在每帧主循环中以非阻塞方式处理延迟任务
5. WHERE 联动模式为手动时，THE Device_Coordinator SHALL 允许各设备独立控制


### 需求 21：主循环与帧调度

**用户故事：** 作为开发者，我希望主循环有明确的执行顺序和时序保证。

#### 验收标准

1. THE Pico_System SHALL 按固定顺序执行每帧：输入采集 → 事件分发 → 界面渲染 → 灯光效果 → 设备联动 → 帧间隔
2. THE Pico_System SHALL 以 sleep_ms(10) 保证最小帧间隔（100fps 上限）
3. THE Pico_System SHALL 在局部刷新时达到 ≥ 30fps 的有效帧率
4. THE Pico_System SHALL 确保输入延迟 < 20ms（编码器中断 + 主循环周期）
5. THE Pico_System SHALL 确保界面切换（含清屏 + 完整绘制）在 100ms 内完成

### 需求 22：内存管理

**用户故事：** 作为开发者，我希望内存使用在预算范围内，不会因内存不足导致崩溃。

#### 验收标准

1. THE Pico_System SHALL 将总 RAM 使用控制在 150KB 以内（264KB 总量 - 80KB 系统 = 184KB 可用，留 34KB 余量）
2. THE Pico_System SHALL 对所有频繁实例化的类使用 __slots__ 减少内存开销
3. THE Pico_System SHALL 使用元组而非字典存储颜色预设数据
4. THE Pico_System SHALL 使用 memoryview 访问位图数据，避免不必要的内存拷贝
5. THE Pico_System SHALL 在界面切换时主动调用 gc.collect() 回收内存
6. THE Pico_System SHALL 实现字体和位图的延迟加载，按需加载而非启动时全部加载

### 需求 23：UI 主题与布局常量

**用户故事：** 作为开发者，我希望颜色、间距、布局参数集中定义，便于统一调整视觉风格。

#### 验收标准

1. THE Pico_System SHALL 在 ui/theme.py 中集中定义所有颜色常量（RGB565 格式）
2. THE Pico_System SHALL 在 ui/theme.py 中定义 76×284 屏幕的三区布局参数：顶栏(Y:0-11, 12px)、主区(Y:12-55, 44px)、底栏(Y:56-75, 20px)
3. THE Pico_System SHALL 在 ui/theme.py 中定义 12 种颜色预设（PRESETS 元组列表）
4. THE Pico_System SHALL 使用模块级常量定义布局参数，避免运行时计算

### 需求 24：可复用控件库

**用户故事：** 作为开发者，我希望常用 UI 元素封装为可复用控件，避免各界面重复实现。

#### 验收标准

1. THE Pico_System SHALL 在 ui/widgets.py 中提供 draw_capsule_bar() 圆角胶囊进度条控件
2. THE Pico_System SHALL 提供 draw_gradient_bar() 渐变胶囊色条控件
3. THE Pico_System SHALL 提供 draw_big_number() 居中大数字显示控件
4. THE Pico_System SHALL 提供 draw_status_dot() 状态指示圆点控件
5. THE Pico_System SHALL 提供 draw_nav_dots() 底部导航圆点指示器控件
6. THE Pico_System SHALL 提供 draw_rgb_channel_bar() RGB 通道条控件（标签+进度条+数值）
7. THE Pico_System SHALL 提供 draw_hint() 底部操作提示文字控件

### 需求 25：系统健壮性与错误恢复

**用户故事：** 作为用户，我希望系统在异常情况下不会失控，能自动恢复。

#### 验收标准

1. WHEN 主循环发生未捕获异常时，THE Pico_System SHALL 调用 stop_all() 停止所有外设，打印错误信息，最多重试 3 次
2. WHEN 字体或位图文件缺失时，THE Pico_System SHALL 回退到内置 8×8 字体和代码绘制的简化图标
3. WHEN 发生 MemoryError 时，THE Pico_System SHALL 强制 gc.collect()，释放字体缓存，重试操作
4. IF 重试仍失败，THEN THE Pico_System SHALL 降级到最小 UI 模式（仅 8px 字体）
5. THE Pico_System SHALL 在 KeyboardInterrupt 时调用 stop_all() 安全停止所有外设
6. THE Pico_System SHALL 按安全顺序初始化：硬件引脚 → 配置加载 → 外设初始化 → UI 初始化 → 开机动画 → 进入菜单