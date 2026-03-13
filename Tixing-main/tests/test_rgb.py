# tests/test_rgb.py — RGB 状态机属性测试
# 属性 6: ∀ rgb_mode ∈ {0,1,2}: 单击事件后 rgb_mode 转换正确

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hypothesis import given, strategies as st, settings
from app_state import AppState
from hal.input import InputEvent, EVT_NONE, EVT_CLICK, EVT_DOUBLE


class MockCtx:
    def __init__(self):
        self.state = AppState()
        self.devices = MockDevices()
        self.config = MockConfig()
        self.screen_manager = MockSM()
        self.input = None
        self.renderer = None
        self.effects = None
        self.coordinator = None

class MockDevices:
    def set_cob1(self, r, g, b): pass
    def set_cob2(self, r, g, b): pass
class MockConfig:
    def save_from_state(self, state): pass
class MockSM:
    def switch(self, ui_id): pass


class TestRGBStateMachine:

    def _make_rgb_screen(self):
        from screens.rgb import RGBScreen
        ctx = MockCtx()
        return RGBScreen(ctx), ctx

    @given(mode=st.sampled_from([0, 1, 2]))
    @settings(max_examples=10)
    def test_click_transitions(self, mode):
        """∀ rgb_mode ∈ {0,1,2}: 单击后转换正确 (0→1, 1→2, 2→1)。"""
        screen, ctx = self._make_rgb_screen()
        ctx.state.rgb_mode = mode

        event = InputEvent()
        event.delta = 0
        event.key = EVT_CLICK
        screen.on_input(event)

        expected = {0: 1, 1: 2, 2: 1}
        assert ctx.state.rgb_mode == expected[mode]

    @given(
        delta=st.integers(min_value=-50, max_value=50),
        strip=st.integers(min_value=0, max_value=1),
        channel=st.integers(min_value=0, max_value=2),
    )
    @settings(max_examples=50)
    def test_rgb_values_always_clamped(self, delta, strip, channel):
        """RGB 值始终在 [0, 255]。"""
        screen, ctx = self._make_rgb_screen()
        ctx.state.rgb_mode = 2
        ctx.state.rgb_strip = strip
        ctx.state.rgb_channel = channel

        event = InputEvent()
        event.delta = delta
        event.key = EVT_NONE
        screen.on_input(event)

        vals = ctx.state.cob1_rgb if strip == 0 else ctx.state.cob2_rgb
        for v in vals:
            assert 0 <= v <= 255

    @given(delta=st.integers(min_value=-10, max_value=10).filter(lambda x: x != 0))
    @settings(max_examples=20)
    def test_strip_always_valid(self, delta):
        """rgb_strip 始终 ∈ {0, 1}。"""
        screen, ctx = self._make_rgb_screen()
        ctx.state.rgb_mode = 0

        event = InputEvent()
        event.delta = delta
        event.key = EVT_NONE
        screen.on_input(event)

        assert ctx.state.rgb_strip in (0, 1)

    @given(delta=st.integers(min_value=-10, max_value=10).filter(lambda x: x != 0))
    @settings(max_examples=20)
    def test_channel_always_valid(self, delta):
        """rgb_channel 始终 ∈ {0, 1, 2}。"""
        screen, ctx = self._make_rgb_screen()
        ctx.state.rgb_mode = 1

        event = InputEvent()
        event.delta = delta
        event.key = EVT_NONE
        screen.on_input(event)

        assert ctx.state.rgb_channel in (0, 1, 2)
