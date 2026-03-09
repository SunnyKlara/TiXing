# effects.py - 灯光效果引擎模块
# 参考 f4 的 LED_GradientProcess()/Streamlight_Process()/Breath_Process() 架构
# 实现 LED 效果优先级系统：normal < preset_gradient < breathing
# 通过参数传递 control 函数，避免循环导入

import math

# --------------------------
# 模块内部状态（效果动画用）
# --------------------------
_breath_phase = 0       # 呼吸灯动画相位计数器
_gradient_step = 0      # 渐变插值步进
_gradient_from_idx = 0  # 渐变源预设索引
_gradient_to_idx = 1    # 渐变目标预设索引


def effects_process(state, set_cob1_fn, set_cob2_fn):
    """
    灯光效果主处理函数，在主循环中每帧调用。
    
    优先级系统（参考 f4 的 deng_2or3/breath_mode 标志位互斥机制）：
      breathing > preset_gradient > normal
    - breath_mode=True → 运行呼吸灯效果（最高优先级）
    - gradient_mode=True → 运行预设渐变效果
    - 否则 → normal 模式，不做任何处理（由界面直接控制 RGB）
    
    :param state: 当前状态字典，需包含 breath_mode, gradient_mode, brightness,
                  cob1_rgb, cob2_rgb, color_preset_idx
    :param set_cob1_fn: control.set_cob1_rgb(r, g, b) 函数引用（参数 0-100）
    :param set_cob2_fn: control.set_cob2_rgb(r, g, b) 函数引用（参数 0-100）
    """
    if state.get("breath_mode", False):
        _breathing_process(state, set_cob1_fn, set_cob2_fn)
    elif state.get("gradient_mode", False):
        _gradient_process(state, set_cob1_fn, set_cob2_fn)
    # else: normal 模式，不处理（RGB 由界面直接通过 update_hardware 控制）


def _breathing_process(state, set_cob1_fn, set_cob2_fn):
    """
    呼吸灯效果（占位实现，完整正弦波逻辑在 Task 5.3）。
    
    参考 f4 的 Breath_Process()：
    - 使用 math.sin() 计算亮度振荡
    - 相位每帧递增 4（628 步 ≈ 2π*100，约 3 秒一个周期 @10ms/帧）
    - 将亮度乘数应用到当前 COB 颜色
    
    :param state: 需包含 brightness, cob1_rgb, cob2_rgb
    """
    global _breath_phase

    # 相位递增（628 步 = 一个完整正弦周期）
    _breath_phase = (_breath_phase + 4) % 628

    # 正弦波亮度乘数：sin 输出 [-1, 1] → 映射到 [0, 1]
    # 参考 f4: sinf(breath_phase * 0.01f)
    sin_val = math.sin(_breath_phase * 0.01)
    multiplier = (sin_val + 1.0) / 2.0  # 0.0 ~ 1.0

    # 应用全局亮度
    brightness = state.get("brightness", 100) / 100.0
    multiplier = multiplier * brightness

    # 将乘数应用到当前 COB 颜色（RGB 0-255 → 0-100）
    cob1 = state.get("cob1_rgb", [0, 0, 0])
    cob2 = state.get("cob2_rgb", [0, 0, 0])

    set_cob1_fn(
        int(cob1[0] * multiplier * 100 // 255),
        int(cob1[1] * multiplier * 100 // 255),
        int(cob1[2] * multiplier * 100 // 255)
    )
    set_cob2_fn(
        int(cob2[0] * multiplier * 100 // 255),
        int(cob2[1] * multiplier * 100 // 255),
        int(cob2[2] * multiplier * 100 // 255)
    )


def _gradient_process(state, set_cob1_fn, set_cob2_fn):
    """
    颜色预设渐变效果（占位实现，完整插值逻辑在 Task 5.2）。
    
    参考 f4 的 Streamlight_Process()：
    - 在 12 种预设之间线性插值
    - 步进每帧递增，到达终点后切换到下一对预设
    
    :param state: 需包含 color_preset_idx, brightness
    """
    global _gradient_step, _gradient_from_idx, _gradient_to_idx

    # 占位：每 100 步切换到下一对预设
    _gradient_step += 1
    if _gradient_step >= 100:
        _gradient_step = 0
        _gradient_from_idx = _gradient_to_idx
        _gradient_to_idx = (_gradient_to_idx + 1) % 12

    # 占位：简单线性插值比例
    t = _gradient_step / 100.0

    # 导入预设数据（延迟导入避免循环依赖）
    from display import COLOR_PRESETS

    preset_from = COLOR_PRESETS[_gradient_from_idx]
    preset_to = COLOR_PRESETS[_gradient_to_idx]

    # 线性插值 COB1
    c1_from = preset_from["cob1"]
    c1_to = preset_to["cob1"]
    r1 = int(c1_from[0] + (c1_to[0] - c1_from[0]) * t)
    g1 = int(c1_from[1] + (c1_to[1] - c1_from[1]) * t)
    b1 = int(c1_from[2] + (c1_to[2] - c1_from[2]) * t)

    # 线性插值 COB2
    c2_from = preset_from["cob2"]
    c2_to = preset_to["cob2"]
    r2 = int(c2_from[0] + (c2_to[0] - c2_from[0]) * t)
    g2 = int(c2_from[1] + (c2_to[1] - c2_from[1]) * t)
    b2 = int(c2_from[2] + (c2_to[2] - c2_from[2]) * t)

    # 应用全局亮度
    brightness = state.get("brightness", 100) / 100.0

    # 输出到 COB（RGB 0-255 → 0-100，带亮度）
    set_cob1_fn(
        int(r1 * brightness * 100 // 255),
        int(g1 * brightness * 100 // 255),
        int(b1 * brightness * 100 // 255)
    )
    set_cob2_fn(
        int(r2 * brightness * 100 // 255),
        int(g2 * brightness * 100 // 255),
        int(b2 * brightness * 100 // 255)
    )


def reset_effects():
    """
    重置所有效果内部状态（相位、步进计数器）。
    在进入 RGB 调色界面时调用，暂停效果让用户直接控制颜色。
    参考 f4 进入 UI3 时关闭 deng_2or3/breath_mode 的行为。
    """
    global _breath_phase, _gradient_step, _gradient_from_idx, _gradient_to_idx
    _breath_phase = 0
    _gradient_step = 0
    _gradient_from_idx = 0
    _gradient_to_idx = 1


def startup_animation(set_cob1_fn, set_cob2_fn, sleep_fn):
    """
    开机动画效果，参考 f4 的 Startup_TaillightFlash()。
    同步阻塞运行，适合启动序列。

    Phase 1 (0~480ms): COB1/COB2 交替白色闪烁 3 次
      - 亮 80ms，灭 80ms，重复 3 次
    Phase 2 (~1500ms): 从黑色线性渐亮到白色
      - 30 步，每步 50ms，亮度从 0 线性到 100

    :param set_cob1_fn: control.set_cob1_rgb(r, g, b)
    :param set_cob2_fn: control.set_cob2_rgb(r, g, b)
    :param sleep_fn: sleep_ms 函数
    """
    # Phase 1: 交替闪烁白色 3 次
    for _ in range(3):
        # COB1 亮，COB2 灭
        set_cob1_fn(100, 100, 100)
        set_cob2_fn(0, 0, 0)
        sleep_fn(80)
        # COB1 灭，COB2 亮
        set_cob1_fn(0, 0, 0)
        set_cob2_fn(100, 100, 100)
        sleep_fn(80)

    # 全灭过渡
    set_cob1_fn(0, 0, 0)
    set_cob2_fn(0, 0, 0)

    # Phase 2: 线性渐亮到白色
    steps = 30
    for i in range(steps + 1):
        val = i * 100 // steps
        set_cob1_fn(val, val, val)
        set_cob2_fn(val, val, val)
        sleep_fn(50)

