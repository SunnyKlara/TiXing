# 需求文档：Pico 功能复刻（UI + 菜单 + 外设控制）

## 简介

本文档定义了在 Raspberry Pi Pico (MicroPython) 平台上复刻 F4 RideWind 项目核心体验的需求。聚焦三个方面：UI 界面视觉效果复刻、菜单系统交互逻辑复刻、以及在现有外设控制基础上的完善。不是完全照搬 F4 代码，而是参考其设计理念，适配 Pico 的 76×284 横屏和 MicroPython 环境。

项目已有基础：8 个界面的状态机框架、基础文字+进度条 UI、PWM 外设控制（风扇/气泵/发烟器/COB 灯带）、编码器输入、配置持久化、灯光效果引擎。

## 术语表

- **Pico_System**: Raspberry Pi Pico 上运行的 MicroPython 主控系统
- **State_Machine**: 界面状态机，基于 ui/init_flag 双变量控制界面切换和刷新
- **Effects_Engine**: 灯光效果引擎，管理呼吸灯、渐变、预设切换等效果
- **Color_Preset**: 12 种预定义的灯带颜色组合
- **Throttle_Mode**: 油门模式，按住编码器按键加速、松开减速的交互模式
- **Device_Coordinator**: 管理多设备联动逻辑的协调器
- **Partial_Refresh**: 局部刷新，仅重绘界面中变化的区域而非全屏重绘

## 需求

### 需求 1：菜单系统复刻

**用户故事：** 作为用户，我希望菜单系统的交互逻辑和视觉风格接近 F4 项目，以便获得流畅直观的导航体验。

#### 验收标准

1. WHEN 菜单界面显示时，THE Pico_System SHALL 以卡片式布局展示当前选中项（居中放大高亮）和左右相邻项（缩小灰色显示）
2. WHEN 编码器旋转切换菜单选项时，THE Pico_System SHALL 以 150ms 防跳间隔限制切换速度，每次仅移动一格
3. WHEN 菜单选中项变化时，THE Pico_System SHALL 在选中项下方显示对应功能的当前值预览（如 "50%"）
4. THE Pico_System SHALL 在菜单底部显示 6 个导航圆点指示器，选中项为强调色圆点，其余为灰色空心圆点
5. WHEN 用户单击编码器时，THE Pico_System SHALL 进入选中项对应的子界面
6. WHEN 用户在子界面双击编码器时，THE Pico_System SHALL 保存配置并返回菜单界面，且菜单选中项保持为上次进入的位置

### 需求 2：速度控制界面 UI 复刻

**用户故事：** 作为用户，我希望风速、发烟、气泵控制界面有统一的视觉风格和清晰的数值显示，以便快速读取和调节参数。

#### 验收标准

1. WHEN 速度控制界面显示时，THE Pico_System SHALL 使用大号数字（3x 放大字体）居中显示当前百分比值
2. WHEN 速度控制界面显示时，THE Pico_System SHALL 在底部显示圆角胶囊形进度条（灰色背景 + 强调色前景）
3. WHEN 速度值变化时，THE Pico_System SHALL 仅重绘数字区域和进度条区域（Partial_Refresh），避免全屏重绘闪烁
4. WHEN 风速界面处于 Throttle_Mode 时，THE Pico_System SHALL 将进度条颜色切换为警告色（橙色）并显示 "THR" 标识
5. THE Pico_System SHALL 在界面右上角显示功能标题（如 "WIND"、"SMOKE"、"PUMP"）
6. THE Pico_System SHALL 在界面固定位置显示操作提示（如双击返回的提示）

### 需求 3：灯光预设界面 UI 复刻

**用户故事：** 作为用户，我希望灯光预设界面能直观展示颜色效果和切换状态，以便快速选择喜欢的灯光方案。

#### 验收标准

1. WHEN 预设界面显示时，THE Pico_System SHALL 在顶部显示预设编号（如 "3/12"）和预设名称
2. WHEN 预设界面显示时，THE Pico_System SHALL 在中间区域显示渐变色条预览（COB1 颜色渐变到 COB2 颜色的胶囊形色条）
3. WHEN 预设为纯色（COB1 和 COB2 颜色相同）时，THE Pico_System SHALL 显示纯色胶囊条而非渐变条
4. WHEN 预设界面显示时，THE Pico_System SHALL 在底部显示 COB1 和 COB2 的颜色对比小方块
5. WHEN 渐变模式开关切换时，THE Pico_System SHALL 通过指示点颜色（绿色=开/红色=关）和文字标签（"GRD"/"FIX"）反映状态
6. WHEN 编码器旋转切换预设时，THE Pico_System SHALL 实时更新色条预览和 COB 灯带输出颜色

### 需求 4：RGB 调色界面 UI 复刻

**用户故事：** 作为用户，我希望 RGB 调色界面能清晰展示三层状态机的当前状态和各通道数值，以便精确调节灯带颜色。

#### 验收标准

1. WHEN RGB 调色界面显示时，THE Pico_System SHALL 在顶部显示当前状态机模式指示（"Strip>"/"Chan>"/"Val>"）和选中灯带名称
2. WHEN RGB 调色界面显示时，THE Pico_System SHALL 显示三行 R/G/B 通道，每行包含通道名、彩色进度条和数值（0-255）
3. WHEN 当前选中的通道变化时，THE Pico_System SHALL 将选中通道的进度条和标签切换为高亮色，未选中通道为暗色
4. WHEN RGB 调色界面显示时，THE Pico_System SHALL 在顶部右侧显示当前灯带颜色的实时预览小色条
5. WHEN 调节 RGB 数值时，THE Pico_System SHALL 以 delta*2 的步进调节（0-255 范围），并实时更新进度条和硬件输出
6. WHEN 状态机模式切换时（单击进入下一层），THE Pico_System SHALL 更新模式指示文字和对应元素的高亮状态

### 需求 5：亮度调节界面 UI 复刻

**用户故事：** 作为用户，我希望亮度调节界面能清晰显示当前亮度和呼吸灯模式状态，以便方便地调节灯光亮度。

#### 验收标准

1. WHEN 亮度界面显示时，THE Pico_System SHALL 使用大号数字居中显示当前亮度百分比
2. WHEN 亮度界面显示时，THE Pico_System SHALL 在底部显示圆角胶囊形亮度进度条
3. WHEN 呼吸灯模式状态变化时，THE Pico_System SHALL 通过指示点（绿色=开/红色=关）和文字（"Breath:ON"/"Breath:OFF"）反映状态
4. WHEN 亮度值变化时，THE Pico_System SHALL 仅重绘数字和进度条区域（Partial_Refresh）

### 需求 6：开机动画与 Logo 界面

**用户故事：** 作为用户，我希望开机时有视觉效果，以便感受到系统正在启动。

#### 验收标准

1. WHEN Pico_System 上电启动时，THE Pico_System SHALL 显示 Logo 界面（深色背景 + 居中品牌文字 + 装饰线）
2. WHEN Logo 界面显示时，THE Pico_System SHALL 同步执行 COB 灯带开机动画（交替闪烁 3 次 + 线性渐亮）
3. WHEN 开机动画完成后，THE Pico_System SHALL 自动切换到菜单界面
4. THE Pico_System SHALL 确保开机动画总时长在 3 秒以内

### 需求 7：局部刷新优化

**用户故事：** 作为开发者，我希望界面刷新采用局部更新策略，以便减少全屏重绘带来的闪烁和性能开销。

#### 验收标准

1. WHEN 界面首次进入时（init_flag=True），THE Pico_System SHALL 执行全屏清屏和完整绘制
2. WHEN 界面参数变化时（init_flag 重置后），THE Pico_System SHALL 仅重绘变化的区域（数字、进度条、状态指示）
3. THE Pico_System SHALL 通过先绘制背景色矩形再绘制新内容的方式清除旧内容残留
4. WHEN 进度条值变化时，THE Pico_System SHALL 重绘整个进度条区域（背景条 + 前景条），避免残影

### 需求 8：油门模式交互完善

**用户故事：** 作为用户，我希望油门模式的操控更平滑，以便获得更好的加减速体验。

#### 验收标准

1. WHILE Throttle_Mode 激活且编码器按键按下时，THE Pico_System SHALL 以非线性加速曲线递增风扇速度（当前速度低于 50 时每帧 +1，高于 50 时每帧 +2）
2. WHILE Throttle_Mode 激活且编码器按键释放时，THE Pico_System SHALL 以每帧 -1 的速率递减风扇速度
3. WHEN Throttle_Mode 下编码器旋转时，THE Pico_System SHALL 退出油门模式并恢复普通调速
4. THE Pico_System SHALL 确保油门模式下风扇速度始终在 0-100 范围内
5. WHEN 进入或退出 Throttle_Mode 时，THE Pico_System SHALL 触发界面重绘以更新油门状态指示

### 需求 9：多设备联动

**用户故事：** 作为用户，我希望相关设备能自动联动，以便减少手动操作。

#### 验收标准

1. WHEN 主风扇速度变化时，THE Device_Coordinator SHALL 按可配置的联动比例同步调整小风扇速度
2. WHEN 发烟器启动时，THE Device_Coordinator SHALL 自动启动气泵以辅助烟雾扩散
3. WHEN 发烟器关闭时，THE Device_Coordinator SHALL 延迟 2 秒后关闭气泵
4. WHERE 联动模式为手动时，THE Device_Coordinator SHALL 允许各设备独立控制
5. THE Device_Coordinator SHALL 在每帧主循环中以非阻塞方式处理延迟任务

### 需求 10：系统健壮性

**用户故事：** 作为开发者，我希望系统有基本的错误恢复能力，以便在异常情况下仍能正常运行。

#### 验收标准

1. WHEN Pico_System 上电启动时，THE Pico_System SHALL 按顺序初始化：硬件引脚 → 配置加载 → 外设初始化 → 开机动画 → 进入菜单
2. IF 配置文件损坏或缺失，THEN THE Pico_System SHALL 使用默认配置值并在串口输出警告
3. WHEN 主循环中发生未捕获异常时，THE Pico_System SHALL 停止所有外设并在串口输出错误信息
4. THE Pico_System SHALL 使用脏标志机制，仅在配置实际变更时写入文件系统
5. WHEN 加载配置时，THE Pico_System SHALL 对每个配置值进行范围校验，超出范围的值替换为默认值
