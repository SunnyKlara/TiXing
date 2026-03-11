# 需求文档：滑动式菜单滚轮界面 (Sliding Menu Wheel)

## 简介

本文档定义滑动式菜单滚轮界面的功能需求，该界面运行在 76×284 横屏 ST7789 LCD 上，通过旋转编码器操控。菜单项以水平滚轮形式排列，选中项居中放大高亮，两侧项缩小淡化，提供直观的导航体验。需求从已批准的设计文档中派生，确保与技术方案一致。

## 术语表

- **MenuWheelScreen**: 滑动式菜单滚轮界面类，继承 Screen 基类，负责菜单项的显示和交互
- **Renderer**: 渲染管线，负责将图形指令输出到 ST7789 LCD 显示屏
- **ScreenManager**: 界面管理器，管理界面切换和生命周期
- **AlphaIconSheet**: 带透明通道的 RLE 压缩图标集加载器
- **InputEvent**: 编码器输入事件对象，包含旋转增量 delta 和按键类型 key
- **calc_visible_items**: 布局计算纯函数，根据选中索引计算可见菜单项的显示参数
- **menu_idx**: 当前选中菜单项的索引值，存储在 AppState 中
- **MENU_ITEMS**: 菜单项定义列表，包含 6 个菜单项（Speed, Smoke, Pump, Color, RGB, Bright）
- **CENTER_X**: 屏幕水平中心坐标，固定值 142
- **MAX_VISIBLE**: 最大可见菜单项数量，固定值 5
- **ITEM_SPACING**: 菜单项中心间距，固定值 64 像素
- **ACCENT**: 强调色，用于高亮选中菜单项
- **导航点**: 屏幕底部的圆点指示器，标识当前选中项在全部菜单项中的位置

## 需求

### 需求 1：菜单项布局计算

**用户故事：** 作为开发者，我希望有一个纯函数根据选中索引计算所有可见菜单项的显示参数，以便实现水平滚轮的视觉布局。

#### 验收标准

1. WHEN calc_visible_items 接收一个有效的 selected_idx 和 total_count 时，THE calc_visible_items SHALL 返回一个长度不超过 MAX_VISIBLE 的列表，每项包含 (item_idx, cx, icon_size, fg_color, font_size)
2. THE calc_visible_items SHALL 将选中项（offset=0）的 cx 设为 CENTER_X（142）
3. WHEN 选中项两侧存在菜单项时，THE calc_visible_items SHALL 将 offset=±1 的项设为中等尺寸（icon_size=20, font_size=8, fg_color=WHITE），将 offset=±2 的项设为小尺寸（icon_size=16, font_size=8, fg_color=GRAY）
4. THE calc_visible_items SHALL 将选中项设为大尺寸（icon_size=32, font_size=16, fg_color=ACCENT）
5. THE calc_visible_items SHALL 仅返回 cx 在 [-ITEM_SPACING//2, SCREEN_W + ITEM_SPACING//2] 范围内的菜单项
6. THE calc_visible_items SHALL 保证返回列表中不存在重复的 item_idx
7. WHEN total_count 小于 MAX_VISIBLE 时，THE calc_visible_items SHALL 使用取模运算实现循环索引

### 需求 2：编码器旋转导航

**用户故事：** 作为用户，我希望旋转编码器时菜单项水平滑动，以便浏览所有菜单选项。

#### 验收标准

1. WHEN 编码器产生正向旋转增量（delta > 0）时，THE MenuWheelScreen SHALL 将 menu_idx 增加 delta 并对 MENU_ITEMS 长度取模
2. WHEN 编码器产生负向旋转增量（delta < 0）时，THE MenuWheelScreen SHALL 将 menu_idx 减少 |delta| 并对 MENU_ITEMS 长度取模
3. THE MenuWheelScreen SHALL 在任意旋转操作后保证 menu_idx 在 [0, len(MENU_ITEMS)) 范围内
4. WHEN menu_idx 发生变化时，THE MenuWheelScreen SHALL 触发界面局部刷新
5. THE MenuWheelScreen SHALL 在处理旋转事件时仅修改 menu_idx，不影响其他 AppState 属性

### 需求 3：单击进入子界面

**用户故事：** 作为用户，我希望单击编码器按键进入当前选中的子界面，以便调整对应功能的参数。

#### 验收标准

1. WHEN 编码器产生单击事件（EVT_CLICK）时，THE MenuWheelScreen SHALL 读取 MENU_ITEMS[menu_idx] 中的目标界面标识
2. WHEN 编码器产生单击事件时，THE MenuWheelScreen SHALL 调用 ScreenManager.switch() 切换到目标子界面
3. THE MenuWheelScreen SHALL 支持 6 个菜单项到对应子界面的映射：Speed→UI_SPEED, Smoke→UI_SMOKE, Pump→UI_PUMP, Color→UI_PRESET, RGB→UI_RGB, Bright→UI_BRIGHT

### 需求 4：菜单项绘制

**用户故事：** 作为用户，我希望看到图标和文字标签组合的菜单项，以便直观识别各功能选项。

#### 验收标准

1. THE MenuWheelScreen SHALL 为每个可见菜单项绘制图标和文字标签，图标在上、标签在下
2. THE MenuWheelScreen SHALL 将图标和标签水平居中于该菜单项的 cx 坐标
3. WHEN AlphaIconSheet 已加载且图标索引有效时，THE MenuWheelScreen SHALL 使用 RLE 图标进行绘制
4. WHEN AlphaIconSheet 未加载或图标索引无效时，THE MenuWheelScreen SHALL 降级为使用菜单项名称首字母作为图标替代
5. THE MenuWheelScreen SHALL 根据菜单项的 font_size 参数选择对应字号的字体绘制标签

### 需求 5：导航点指示器

**用户故事：** 作为用户，我希望在屏幕底部看到导航点指示器，以便了解当前选中项在全部菜单中的位置。

#### 验收标准

1. THE MenuWheelScreen SHALL 在屏幕底部（y=DOTS_Y）绘制与 MENU_ITEMS 数量相同的导航点
2. THE MenuWheelScreen SHALL 将当前选中项对应的导航点以高亮样式绘制，其余导航点以普通样式绘制
3. THE MenuWheelScreen SHALL 将导航点组水平居中于屏幕

### 需求 6：图标资源生命周期管理

**用户故事：** 作为开发者，我希望图标资源在界面进入时加载、退出时释放，以便在 RP2040 有限内存下安全运行。

#### 验收标准

1. WHEN MenuWheelScreen 的 on_enter() 被调用时，THE MenuWheelScreen SHALL 尝试加载 AlphaIconSheet 图标资源
2. WHEN MenuWheelScreen 的 on_exit() 被调用时，THE MenuWheelScreen SHALL 将图标资源引用设为 None 以释放内存
3. IF 图标资源文件不存在或格式错误，THEN THE MenuWheelScreen SHALL 将 _icons 设为 None 并打印警告日志
4. IF 加载图标时发生 MemoryError，THEN THE MenuWheelScreen SHALL 捕获异常，将 _icons 设为 None，并降级为无图标模式

### 需求 7：绘制策略

**用户故事：** 作为开发者，我希望界面支持全屏绘制和局部刷新两种模式，以便在保证视觉完整性的同时优化渲染性能。

#### 验收标准

1. WHEN draw_full() 被调用时，THE MenuWheelScreen SHALL 清除整个屏幕并绘制所有可见菜单项和导航点
2. WHEN draw_update() 被调用且 menu_idx 已变化时，THE MenuWheelScreen SHALL 仅清除内容区域（不含导航点区域）并重绘可见菜单项和导航点
3. WHEN draw_update() 被调用且 menu_idx 未变化时，THE MenuWheelScreen SHALL 跳过重绘以节省渲染时间
4. THE MenuWheelScreen SHALL 按从两侧到中心的顺序绘制菜单项，确保选中项在最上层

### 需求 8：循环滚动

**用户故事：** 作为用户，我希望菜单在首尾项之间无缝循环滚动，以便快速到达任意菜单项。

#### 验收标准

1. WHEN menu_idx 为 0 且编码器产生负向旋转时，THE MenuWheelScreen SHALL 将 menu_idx 设为 len(MENU_ITEMS) - 1
2. WHEN menu_idx 为 len(MENU_ITEMS) - 1 且编码器产生正向旋转时，THE MenuWheelScreen SHALL 将 menu_idx 设为 0
3. THE calc_visible_items SHALL 在计算可见项时使用取模运算，使首尾两端的菜单项自然出现在可见范围内
