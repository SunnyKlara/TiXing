# test_effects.py - effects.py 单元测试
# 可在 CPython 上运行验证逻辑正确性

import effects


def make_state(**overrides):
    """构建测试用状态字典"""
    state = {
        "breath_mode": False,
        "gradient_mode": False,
        "brightness": 100,
        "cob1_rgb": [255, 0, 0],
        "cob2_rgb": [0, 0, 255],
        "color_preset_idx": 0,
    }
    state.update(overrides)
    return state


class MockCob:
    """记录 set_cob_rgb 调用参数"""
    def __init__(self):
        self.calls = []

    def __call__(self, r, g, b):
        self.calls.append((r, g, b))

    @property
    def last(self):
        return self.calls[-1] if self.calls else None


def test_normal_mode_does_nothing():
    """normal 模式（breath_mode=False, gradient_mode=False）不应调用 set_cob"""
    effects.reset_effects()
    cob1 = MockCob()
    cob2 = MockCob()
    state = make_state()
    effects.effects_process(state, cob1, cob2)
    assert len(cob1.calls) == 0
    assert len(cob2.calls) == 0
    print("PASS: test_normal_mode_does_nothing")


def test_breathing_overrides_gradient():
    """breath_mode=True 时应运行呼吸灯，即使 gradient_mode 也为 True（优先级互斥）"""
    effects.reset_effects()
    cob1_breath = MockCob()
    cob2_breath = MockCob()
    state = make_state(breath_mode=True, gradient_mode=True)
    effects.effects_process(state, cob1_breath, cob2_breath)
    # 呼吸灯应调用 set_cob
    assert len(cob1_breath.calls) == 1
    assert len(cob2_breath.calls) == 1
    print("PASS: test_breathing_overrides_gradient")


def test_gradient_runs_when_no_breathing():
    """gradient_mode=True 且 breath_mode=False 时应运行渐变"""
    effects.reset_effects()
    cob1 = MockCob()
    cob2 = MockCob()
    state = make_state(gradient_mode=True)
    effects.effects_process(state, cob1, cob2)
    assert len(cob1.calls) == 1
    assert len(cob2.calls) == 1
    print("PASS: test_gradient_runs_when_no_breathing")


def test_breathing_produces_varying_output():
    """呼吸灯多帧调用应产生不同的亮度值（正弦波变化）"""
    effects.reset_effects()
    state = make_state(breath_mode=True, cob1_rgb=[255, 255, 255], cob2_rgb=[255, 255, 255])
    outputs = []
    for _ in range(20):
        cob1 = MockCob()
        cob2 = MockCob()
        effects.effects_process(state, cob1, cob2)
        outputs.append(cob1.last[0])  # 记录 R 通道值
    # 20 帧内应有不同的值（正弦波在变化）
    unique_vals = set(outputs)
    assert len(unique_vals) > 1, "呼吸灯应产生变化的亮度值"
    print("PASS: test_breathing_produces_varying_output")


def test_breathing_respects_brightness():
    """呼吸灯应受全局亮度影响：brightness=50 时输出应小于 brightness=100"""
    effects.reset_effects()
    state_full = make_state(breath_mode=True, brightness=100,
                            cob1_rgb=[255, 255, 255], cob2_rgb=[255, 255, 255])
    # 运行几帧让相位到一个较高值
    for _ in range(50):
        cob1 = MockCob()
        effects.effects_process(state_full, cob1, MockCob())
    val_full = cob1.last[0]

    # 重置并用 brightness=50 运行相同帧数
    effects.reset_effects()
    state_half = make_state(breath_mode=True, brightness=50,
                            cob1_rgb=[255, 255, 255], cob2_rgb=[255, 255, 255])
    for _ in range(50):
        cob1_half = MockCob()
        effects.effects_process(state_half, cob1_half, MockCob())
    val_half = cob1_half.last[0]

    # brightness=50 的输出应 <= brightness=100 的输出
    assert val_half <= val_full, f"brightness=50 ({val_half}) should be <= brightness=100 ({val_full})"
    print("PASS: test_breathing_respects_brightness")


def test_breathing_output_in_range():
    """呼吸灯输出值应在 0-100 范围内（control.py 期望 0-100）"""
    effects.reset_effects()
    state = make_state(breath_mode=True, cob1_rgb=[255, 255, 255], cob2_rgb=[255, 255, 255])
    for _ in range(200):
        cob1 = MockCob()
        cob2 = MockCob()
        effects.effects_process(state, cob1, cob2)
        r1, g1, b1 = cob1.last
        r2, g2, b2 = cob2.last
        for v in [r1, g1, b1, r2, g2, b2]:
            assert 0 <= v <= 100, f"输出值 {v} 超出 0-100 范围"
    print("PASS: test_breathing_output_in_range")


def test_gradient_output_in_range():
    """渐变效果输出值应在 0-100 范围内"""
    effects.reset_effects()
    state = make_state(gradient_mode=True)
    for _ in range(200):
        cob1 = MockCob()
        cob2 = MockCob()
        effects.effects_process(state, cob1, cob2)
        r1, g1, b1 = cob1.last
        r2, g2, b2 = cob2.last
        for v in [r1, g1, b1, r2, g2, b2]:
            assert 0 <= v <= 100, f"输出值 {v} 超出 0-100 范围"
    print("PASS: test_gradient_output_in_range")


def test_reset_effects():
    """reset_effects 应重置所有内部状态"""
    # 运行一些帧让内部状态变化
    state = make_state(breath_mode=True, cob1_rgb=[255, 0, 0], cob2_rgb=[0, 0, 255])
    for _ in range(50):
        effects.effects_process(state, MockCob(), MockCob())

    # 重置
    effects.reset_effects()
    assert effects._breath_phase == 0
    assert effects._gradient_step == 0
    assert effects._gradient_from_idx == 0
    assert effects._gradient_to_idx == 1
    print("PASS: test_reset_effects")


def test_gradient_cycles_through_presets():
    """渐变效果应在预设之间循环（100 步后切换到下一对）"""
    effects.reset_effects()
    state = make_state(gradient_mode=True)
    # 运行 101 帧，应切换到下一对预设
    for _ in range(101):
        effects.effects_process(state, MockCob(), MockCob())
    assert effects._gradient_from_idx == 1
    assert effects._gradient_to_idx == 2
    print("PASS: test_gradient_cycles_through_presets")


def test_startup_animation_phase1_alternates():
    """开机动画 Phase 1 应交替闪烁 COB1/COB2 白色 3 次"""
    cob1 = MockCob()
    cob2 = MockCob()
    sleeps = []
    effects.startup_animation(cob1, cob2, lambda ms: sleeps.append(ms))
    # Phase 1: 3 次循环，每次 COB1亮+COB2灭 然后 COB1灭+COB2亮 = 6 对调用
    # 调用顺序: (cob1亮,cob2灭), (cob1灭,cob2亮) × 3, 然后全灭, 然后 Phase 2 的 31 步
    assert cob1.calls[0] == (100, 100, 100), f"第1次 COB1 应亮白: {cob1.calls[0]}"
    assert cob2.calls[0] == (0, 0, 0), f"第1次 COB2 应灭: {cob2.calls[0]}"
    assert cob1.calls[1] == (0, 0, 0), f"第2次 COB1 应灭: {cob1.calls[1]}"
    assert cob2.calls[1] == (100, 100, 100), f"第2次 COB2 应亮白: {cob2.calls[1]}"
    # 第3对（第2次循环）
    assert cob1.calls[2] == (100, 100, 100)
    assert cob2.calls[3] == (100, 100, 100)
    print("PASS: test_startup_animation_phase1_alternates")


def test_startup_animation_phase2_ramps_up():
    """开机动画 Phase 2 应从 0 线性渐亮到 100"""
    cob1 = MockCob()
    cob2 = MockCob()
    effects.startup_animation(cob1, cob2, lambda ms: None)
    # Phase 1: 6 对 + 1 全灭 = 7 次 COB1 调用
    # Phase 2: 31 步 (0..30)
    phase2_start = 7
    assert cob1.calls[phase2_start] == (0, 0, 0), f"Phase 2 起始应为 0: {cob1.calls[phase2_start]}"
    assert cob1.calls[-1] == (100, 100, 100), f"Phase 2 结束应为 100: {cob1.calls[-1]}"
    # 应单调递增
    prev = -1
    for i in range(phase2_start, len(cob1.calls)):
        val = cob1.calls[i][0]
        assert val >= prev, f"Phase 2 应单调递增: step {i-phase2_start} val={val} < prev={prev}"
        prev = val
    print("PASS: test_startup_animation_phase2_ramps_up")


def test_startup_animation_output_in_range():
    """开机动画所有输出值应在 0-100 范围内"""
    cob1 = MockCob()
    cob2 = MockCob()
    effects.startup_animation(cob1, cob2, lambda ms: None)
    for i, (r, g, b) in enumerate(cob1.calls):
        for v in [r, g, b]:
            assert 0 <= v <= 100, f"COB1 call {i}: 值 {v} 超出 0-100"
    for i, (r, g, b) in enumerate(cob2.calls):
        for v in [r, g, b]:
            assert 0 <= v <= 100, f"COB2 call {i}: 值 {v} 超出 0-100"
    print("PASS: test_startup_animation_output_in_range")


def test_startup_animation_total_sleep():
    """开机动画总时长约 2 秒（480ms 闪烁 + ~1550ms 渐亮）"""
    sleeps = []
    effects.startup_animation(MockCob(), MockCob(), lambda ms: sleeps.append(ms))
    total = sum(sleeps)
    # Phase 1: 6 × 80ms = 480ms, Phase 2: 31 × 50ms = 1550ms, 总计 2030ms
    assert 1900 <= total <= 2200, f"总时长 {total}ms 应在 1900-2200ms 范围"
    print("PASS: test_startup_animation_total_sleep")


if __name__ == "__main__":
    test_normal_mode_does_nothing()
    test_breathing_overrides_gradient()
    test_gradient_runs_when_no_breathing()
    test_breathing_produces_varying_output()
    test_breathing_respects_brightness()
    test_breathing_output_in_range()
    test_gradient_output_in_range()
    test_reset_effects()
    test_gradient_cycles_through_presets()
    test_startup_animation_phase1_alternates()
    test_startup_animation_phase2_ramps_up()
    test_startup_animation_output_in_range()
    test_startup_animation_total_sleep()
    print("\n全部测试通过!")
