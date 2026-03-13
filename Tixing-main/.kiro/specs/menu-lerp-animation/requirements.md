# 需求文档：菜单线性插值滑动过渡动画 (Menu Lerp Animation)

## 简介

本文档定义菜单滚轮界面（MenuWheelScreen）的线性插值（lerp）滑动过渡动画需求。当用户旋转编码器切换菜单项时，所有可见元素从当前位置平滑过渡到目标位置，取代现有的瞬间跳变行为。动画采用整数定点数运算，参考 OLED 菜单演示代码 Animation.c 的实现方式。

## 术语表

- **LerpAnimator**: 线性插值动画控制器，负责管理多元素同步动画的预计算和帧推进
- **MenuWheelScreen**: 水平滚轮菜单界面，集成动画控制器实现平滑过渡
- **定点数 (Fixed-Point)**: 使用左移 8 位（<<8）的整数表示小数，避免浮点运算
- **FP_SHIFT**: 定点数移位位数，固定为 8
- **槽位 (Slot)**: 菜单中一个可见元素的位置和尺寸信息，包含 cx（X 中心坐标）、icon_w（图标宽度）、icon_h（图标高度）
- **步进值 (Step)**: 每帧动画中各属性的增量值，预计算为 `(target - start) // frames`（定点数域）
- **吸附 (Snap)**: 动画最后一帧强制设置为精确目标值，消除累积误差
- **布局快照 (Layout Snapshot)**: 捕获某一时刻所有可见槽位的位置和尺寸状态
- **MAX_SLOTS**: 最大可见槽位数，固定为 5
- **ANIM_FRAMES**: 动画总帧数，默认为 6，可配置
- **编码器 (Encoder)**: 旋转编码器输入设备，产生 delta 旋转事件和 click 按键事件

## 需求

### 需求 1：动画控制器核心功能

**用户故事：** 作为开发者，我希望有一个独立的动画控制器模块，以便管理多元素同步线性插值动画。

#### 验收标准

1. WHEN `LerpAnimator.start()` 被调用并传入起始槽位列表、目标槽位列表和帧数时，THE LerpAnimator SHALL 为每个槽位的每个属性（cx、icon_w、icon_h）预计算定点数步进值
2. WHEN `LerpAnimator.start()` 被调用时，THE LerpAnimator SHALL 将帧计数器重置为 0 并将 `is_active` 设为 True
3. WHEN 帧数参数小于 2 时，THE LerpAnimator SHALL 将帧数钳制到最小值 2
4. THE LerpAnimator SHALL 使用左移 FP_SHIFT 位的整数定点数进行所有坐标运算，不使用浮点运算
5. THE LerpAnimator SHALL 在初始化时预分配固定大小的内部数组，动画期间不进行内存分配

### 需求 2：帧推进与最终值精度

**用户故事：** 作为开发者，我希望动画帧推进过程精确可靠，以便动画结束后元素位于精确的目标位置。

#### 验收标准

1. WHEN `LerpAnimator.tick()` 在中间帧被调用时，THE LerpAnimator SHALL 对每个槽位的每个属性累加步进值，并返回转换为屏幕坐标的中间位置列表
2. WHEN `LerpAnimator.tick()` 在最后一帧（frame >= total_frames）被调用时，THE LerpAnimator SHALL 返回精确的目标值并将 `is_active` 设为 False
3. WHEN 连续调用 `tick()` 恰好 N 次（N 为 start 时指定的帧数）后，THE LerpAnimator SHALL 将 `is_active` 从 True 变为 False
4. WHEN 起始值 s 小于目标值 t 时，THE LerpAnimator SHALL 保证中间帧的对应属性值序列单调递增
5. WHEN 起始值 s 大于目标值 t 时，THE LerpAnimator SHALL 保证中间帧的对应属性值序列单调递减

### 需求 3：布局快照生成

**用户故事：** 作为开发者，我希望能捕获菜单在任意选中项下的布局状态，以便为动画提供起始和目标位置数据。

#### 验收标准

1. WHEN `snapshot_layout()` 被调用并传入有效的 `selected_idx` 和 `total_count` 时，THE snapshot_layout 函数 SHALL 返回长度恰好为 MAX_SLOTS 的列表
2. WHEN 生成布局快照时，THE snapshot_layout 函数 SHALL 为每个槽位返回 (cx, icon_w, icon_h) 三元组
3. WHEN 生成布局快照时，THE snapshot_layout 函数 SHALL 将中心槽位（索引 2）的 cx 设为 CENTER_X
4. WHEN 槽位对应的菜单项在屏幕可见范围外时，THE snapshot_layout 函数 SHALL 使用屏幕外坐标表示该槽位

### 需求 4：菜单界面动画集成

**用户故事：** 作为用户，我希望旋转编码器时菜单项平滑滑动到新位置，而非瞬间跳变，以获得流畅的视觉体验。

#### 验收标准

1. WHEN 用户旋转编码器且当前无动画进行时，THE MenuWheelScreen SHALL 更新 menu_idx、捕获起始和目标布局快照，并启动 LerpAnimator 动画
2. WHEN 动画处于活跃状态时，THE MenuWheelScreen SHALL 在 `draw_update()` 中调用 `tick()` 推进帧并绘制中间状态
3. WHEN 动画完成后，THE MenuWheelScreen SHALL 恢复为正常的静态绘制逻辑
4. WHEN 动画完成后，THE MenuWheelScreen SHALL 产生与直接调用静态布局计算函数一致的视觉结果

### 需求 5：动画期间输入处理

**用户故事：** 作为用户，我希望动画期间的操作不会导致界面状态混乱，以保证交互的可靠性。

#### 验收标准

1. WHILE 动画处于活跃状态，THE MenuWheelScreen SHALL 忽略编码器旋转事件，不改变 menu_idx
2. WHEN 动画完成后，THE MenuWheelScreen SHALL 自动恢复对编码器旋转事件的响应
3. WHEN 用户在动画期间按下编码器按键（EVT_CLICK）时，THE MenuWheelScreen SHALL 忽略该按键事件

### 需求 6：动画配置与性能

**用户故事：** 作为开发者，我希望动画帧数可配置且运算高效，以便在不同性能需求下调优。

#### 验收标准

1. THE LerpAnimator SHALL 支持通过 ANIM_FRAMES 常量配置动画总帧数，默认值为 6
2. WHEN ANIM_FRAMES 值较小时，THE LerpAnimator SHALL 产生更短的动画时长
3. WHEN ANIM_FRAMES 值较大时，THE LerpAnimator SHALL 产生更长更平滑的动画时长
4. THE LerpAnimator SHALL 使用扁平列表存储槽位数据，每个槽位占用固定 SLOT_STRIDE 个整数
5. THE LerpAnimator SHALL 在 `tick()` 中仅执行整数加法和位移运算，不执行乘法、除法或浮点运算

### 需求 7：模块结构与向后兼容

**用户故事：** 作为开发者，我希望动画功能作为独立模块添加，不破坏现有接口，以便安全集成。

#### 验收标准

1. THE LerpAnimator 和 snapshot_layout SHALL 作为独立模块 `ui/lerp.py` 提供
2. THE MenuWheelScreen SHALL 保持现有的 `on_enter()`、`on_exit()`、`draw_full()` 接口不变
3. THE MenuWheelScreen SHALL 保持 `MenuScreen = MenuWheelScreen` 的向后兼容别名
4. WHEN LerpAnimator 未被使用或动画未活跃时，THE MenuWheelScreen SHALL 保持与修改前完全一致的静态绘制行为

### 需求 8：错误处理与健壮性

**用户故事：** 作为开发者，我希望动画系统能优雅处理异常情况，不导致界面崩溃。

#### 验收标准

1. IF 图标资源加载失败，THEN THE MenuWheelScreen SHALL 使用文字首字母作为回退显示，动画仍正常执行
2. IF 动画期间发生渲染异常，THEN THE MenuWheelScreen SHALL 终止动画并恢复到静态绘制状态
3. WHEN `start()` 接收到的起始和目标槽位列表长度不等于 MAX_SLOTS 时，THE LerpAnimator SHALL 拒绝启动动画并保持 Idle 状态
