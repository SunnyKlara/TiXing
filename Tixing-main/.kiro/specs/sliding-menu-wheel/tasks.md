# 实现计划：滑动式菜单滚轮界面 (Sliding Menu Wheel)

## 概述

将现有的单项居中菜单界面替换为水平滑动式菜单滚轮。选中项居中放大高亮，两侧项缩小淡化，形成"滚轮"视觉效果。实现范围仅涉及 `screens/menu.py` 模块的重写，复用现有的 Screen 基类、Renderer、AlphaIconSheet、ScreenManager 等基础设施。先用 `tools/generate_menu_icons.py` 生成占位图标，后续可直接替换 PNG 资源。

## 任务

- [x] 1. 实现布局计算纯函数 calc_visible_items
  - [x] 1.1 在 `screens/menu.py` 中实现 `calc_visible_items(selected_idx, total_count)` 纯函数
    - 根据 selected_idx 计算最多 MAX_VISIBLE 个可见菜单项的显示参数
    - 返回 `[(item_idx, cx, icon_size, fg_color, font_size), ...]`
    - 选中项 (offset=0): cx=CENTER_X(142), icon_size=32, fg_color=ACCENT, font_size=16
    - offset=±1: icon_size=20, fg_color=WHITE, font_size=8
    - offset=±2: icon_size=16, fg_color=GRAY, font_size=8
    - 使用取模运算实现循环索引，过滤超出屏幕范围的项
    - _需求: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 8.3_

  - [x]* 1.2 编写 calc_visible_items 的属性测试 (Property 1: 可见项数量上界)
    - **属性 1: 可见项数量上界**
    - 使用 hypothesis，对任意 selected_idx ∈ [0, total_count) 和 total_count > 0，验证返回列表长度 ≤ MAX_VISIBLE
    - **验证需求: 1.1**

  - [x]* 1.3 编写 calc_visible_items 的属性测试 (Property 2: 选中项居中且参数正确)
    - **属性 2: 选中项居中且参数正确**
    - 对任意有效 selected_idx，验证返回列表中恰好有一项 fg_color==ACCENT，且 cx==CENTER_X, icon_size==32, font_size==16
    - **验证需求: 1.2, 1.4**

  - [x]* 1.4 编写 calc_visible_items 的属性测试 (Property 3: 尺寸梯度)
    - **属性 3: 尺寸梯度**
    - 对任意有效 selected_idx，验证 offset=±1 项为 (icon_size=20, WHITE, font_size=8)，offset=±2 项为 (icon_size=16, GRAY, font_size=8)
    - **验证需求: 1.3**

  - [x]* 1.5 编写 calc_visible_items 的属性测试 (Property 4: 可见项坐标边界)
    - **属性 4: 可见项坐标边界**
    - 对任意有效 selected_idx，验证所有返回项的 cx 在 [-ITEM_SPACING//2, SCREEN_W + ITEM_SPACING//2] 范围内
    - **验证需求: 1.5**

  - [x]* 1.6 编写 calc_visible_items 的属性测试 (Property 5: 可见项索引唯一性)
    - **属性 5: 可见项索引唯一性**
    - 对任意有效 selected_idx，验证返回列表中不存在重复的 item_idx
    - **验证需求: 1.6**

- [x] 2. 实现 MenuWheelScreen 核心类
  - [x] 2.1 定义布局常量和 MENU_ITEMS 数据
    - 在 `screens/menu.py` 顶部定义 SCREEN_W, SCREEN_H, ITEM_SPACING, CENTER_X, ICON_Y, LABEL_Y_BIG, LABEL_Y_SMALL, DOTS_Y, MAX_VISIBLE 等常量
    - 更新 MENU_ITEMS 列表，所有 6 个菜单项的图标索引设为 0-5（对应 generate_menu_icons.py 生成的图标）
    - _需求: 3.3, 4.1_

  - [x] 2.2 实现 MenuWheelScreen 类框架（__init__, on_enter, on_exit）
    - 继承 Screen 基类
    - `__init__`: 初始化 _icons=None, _prev_idx=-1
    - `on_enter`: 尝试加载 AlphaIconSheet，失败时降级为 None 并打印警告；捕获 MemoryError
    - `on_exit`: 将 _icons 设为 None 释放内存
    - _需求: 6.1, 6.2, 6.3, 6.4_

  - [x] 2.3 实现 on_input 事件处理
    - 旋转事件: menu_idx = (menu_idx + delta) % len(MENU_ITEMS)
    - 单击事件: 读取 MENU_ITEMS[menu_idx][1] 作为目标界面，调用 screen_manager.switch()
    - _需求: 2.1, 2.2, 2.3, 2.5, 3.1, 3.2, 3.3, 8.1, 8.2_

  - [x]* 2.4 编写 on_input 的属性测试 (Property 6: 菜单索引边界不变量)
    - **属性 6: 菜单索引边界不变量**
    - 使用 hypothesis 生成任意 delta 序列，验证每次旋转后 menu_idx 始终在 [0, len(MENU_ITEMS)) 范围内
    - **验证需求: 2.1, 2.2, 2.3, 8.1, 8.2**

  - [x]* 2.5 编写 on_input 的属性测试 (Property 7: 旋转事件状态隔离)
    - **属性 7: 旋转事件状态隔离**
    - 对任意旋转事件，验证 AppState 中除 menu_idx 外的所有属性保持不变
    - **验证需求: 2.5**

  - [x]* 2.6 编写 on_input 的属性测试 (Property 8: 单击映射正确性)
    - **属性 8: 单击映射正确性**
    - 对任意有效 menu_idx，验证单击事件触发 ScreenManager.switch() 且参数等于 MENU_ITEMS[menu_idx][1]
    - **验证需求: 3.1, 3.2**

- [x] 3. 检查点 - 确保核心逻辑测试通过
  - 确保所有测试通过，如有疑问请询问用户。

- [x] 4. 实现绘制逻辑
  - [x] 4.1 实现 _draw_item 单项绘制方法
    - 根据 icon_size 和 fg_color 绘制图标（AlphaIconSheet 或首字母 fallback）
    - 根据 font_size 绘制文字标签，水平居中于 cx
    - 选中项标签 y=LABEL_Y_BIG，侧边项 y=LABEL_Y_SMALL
    - _需求: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [x] 4.2 实现 draw_full 全屏绘制
    - 清除整个屏幕
    - 调用 calc_visible_items 获取可见项
    - 按从两侧到中心的顺序绘制（先 side_items 后 center_item）
    - 调用 draw_nav_dots 绘制底部导航点
    - _需求: 5.1, 5.2, 5.3, 7.1, 7.4_

  - [x] 4.3 实现 draw_update 局部刷新
    - 仅在 menu_idx 变化时重绘
    - 清除内容区域（0 到 DOTS_Y-2），不含导航点区域
    - 重绘可见菜单项和导航点
    - menu_idx 未变化时跳过重绘
    - _需求: 2.4, 7.2, 7.3, 7.4_

  - [x]* 4.4 编写绘制逻辑的单元测试
    - 测试 _draw_item 在有图标和无图标模式下的行为
    - 测试 draw_full 和 draw_update 的调用流程
    - 测试 draw_update 在 menu_idx 未变化时跳过重绘
    - _需求: 4.3, 4.4, 7.2, 7.3_

- [x] 5. 生成占位图标资源并集成
  - [x] 5.1 运行 `tools/generate_menu_icons.py` 生成占位图标
    - 确保 `assets/menu_icons_alpha.bin` 文件生成成功
    - 验证生成的图标集包含 6 个 40×40 图标
    - _需求: 4.3, 6.1_

  - [x] 5.2 在 main.py 中注册 MenuWheelScreen 替换旧的 MenuScreen
    - 将 `from screens.menu import MenuScreen` 改为 `from screens.menu import MenuWheelScreen`
    - 将 `MenuScreen(ctx)` 改为 `MenuWheelScreen(ctx)`
    - 确保注册到 UI_MENU
    - _需求: 3.3_

- [x] 6. 最终检查点 - 确保所有测试通过
  - 确保所有测试通过，如有疑问请询问用户。

## 备注

- 标记 `*` 的任务为可选，可跳过以加速 MVP 开发
- 每个任务引用了具体的需求编号，确保可追溯性
- 属性测试使用 hypothesis 库，验证设计文档中定义的正确性属性
- 占位图标由 `tools/generate_menu_icons.py` 程序化生成，后续可直接替换 PNG 资源无需改代码
- 现有 `screens/menu.py` 将被完整替换，类名从 `MenuScreen` 改为 `MenuWheelScreen`
